// Copyright (c) 2015 fjz13. All rights reserved.
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file.
#include "MedusaPreCompiled.h"
#include "IScene.h"
#include "Resource/RenderTarget/IRenderTarget.h"
#include "Resource/Camera/Camera.h"
#include "Graphics/ResolutionAdapter.h"
#include "Resource/RenderTarget/RenderTargetFactory.h"
#include "Node/Editor/NodeEditorFactory.h"
#include "Core/Command/Executor/SyncCommandExecutor.h"
#include "Node/Layer/ILayer.h"
#include "Core/Log/Log.h"
#include "Application/ApplicationStatics.h"
#include "Core/Pattern/IVisitor.h"
#include "Resource/Model/Mesh/IMesh.h"
#include "Node/NodeSweeper.h"
#include "Core/IO/Path.h"
#include "Core/IO/FileInfo.h"
#include "Node/NodeFactory.h"
#include "Node/Input/InputDispatcher.h"

MEDUSA_BEGIN;


IScene::IScene(const StringRef& name/*=StringRef::Empty*/, const IEventArg& e /*= IEventArg::Empty*/)
	:INode(name), mSceneGraph(nullptr)
{
	mScene = this;
	mRenderTarget = nullptr;

	mSize = ResolutionAdapter::Instance().WinSize();

	SetStretch(ResolutionAdapter::Instance().GetStretch());
	//SetSizeToContent(SizeToContent::WidthAndHeight);
	Start();

	MutableInput().Enable(true);	//default enable input
}


IScene::~IScene(void)
{
	Uninitialize();
	
}

bool IScene::Uninitialize()
{
	SAFE_DELETE(mSceneGraph);
	return true;
}


const Share<IRenderTarget>& IScene::RenderTarget() const
{
	if (mRenderTarget == nullptr)
	{
		return RenderTargetFactory::Instance().Current();
	}
	return mRenderTarget;
}


void IScene::SetRenderTarget(const Share<IRenderTarget>& val)
{
	RETURN_IF_EQUAL(mRenderTarget, val);
	mRenderTarget = val;
	OnRenderQueueChanged();
}

void IScene::SetCamera(const Share<Camera>& val)
{
	RETURN_IF_EQUAL(mCamera, val);
	mCamera = val;
	
	OnRenderQueueChanged();
}
//////////////////////////////////////////////////////////////////////////

bool IScene::DeleteLayer(StringRef name, NodeDeleteFlags deleteFlags/*=LayerDeleteFlags::None*/)
{
	if (MEDUSA_FLAG_HAS(deleteFlags,NodeDeleteFlags::Async))
	{
		INode* layer = FindChild(name);
		if (layer != nullptr)
		{
			NodeSweeper::Instance().Add(layer);
		}
		return layer != nullptr;
	}
	else
	{
		return DeleteChild(name);
	}

}


bool IScene::DeleteLayer(ILayer* layer, NodeDeleteFlags deleteFlags /*= LayerDeleteFlags::None*/)
{
	if (MEDUSA_FLAG_HAS(deleteFlags,NodeDeleteFlags::Async))
	{
		RemoveChild(layer);
		NodeSweeper::Instance().Add(layer);
		return layer != nullptr;
	}
	else
	{
		return DeleteChild(layer);
	}
}

void IScene::DeleteLayers(const List<StringRef>& names, NodeDeleteFlags deleteFlags/*=LayerDeleteFlags::None*/)
{
	for(auto i: names)
	{
		DeleteLayer(i, deleteFlags);
	}
}

//////////////////////////////////////////////////////////////////////////

ILayer* IScene::CurrentLayer() const
{
	RETURN_NULL_IF_EMPTY(mNodes);
	return (ILayer*)mNodes.Last();
}



ILayer* IScene::PushLayer(ILayer* layer, NodePushFlags pushFlags/*=NodePushFlags::None*/)
{
	Log::Assert(layer != nullptr, "thisLayer should not be null.");
    RETURN_NULL_IF_NULL(layer);

#ifdef MEDUSA_SAFE_CHECK
	if (HasChild(layer))
	{
		Log::AssertFailed("The layer pushed is already in layer stack.");
		return layer;
	}
#endif

	if (MEDUSA_FLAG_HAS(pushFlags,NodePushFlags::SaveStatusBeforePush))
	{
		OnSaveStatus();
	}


	ILayer* originalLayer = CurrentLayer();
	if (originalLayer != nullptr)
	{
		if (MEDUSA_FLAG_HAS(pushFlags,NodePushFlags::HideAllPrevs))
		{
			for(auto prevLayer: mNodes)
			{
				if (prevLayer->IsVisible())
				{
					prevLayer->SetVisible(false);
					prevLayer->ExitRecursively();
					prevLayer->MutableInput().Enable(false);
				}
			}
		}

		if (!MEDUSA_FLAG_HAS(pushFlags,NodePushFlags::ShowPrev))
		{
			originalLayer->SetVisible(false);
			originalLayer->ExitRecursively();
		}
		originalLayer->MutableInput().Enable(MEDUSA_FLAG_HAS(pushFlags,NodePushFlags::ShowPrev));
	}

	AddChild(layer);

	layer->SetVisible(true);
	layer->EnterRecursively();

	if (!MEDUSA_FLAG_HAS(pushFlags,NodePushFlags::SuppressUpdateLogic))
	{
		layer->UpdateLogicRecursively();
	}

	layer->MutableInput().Enable(!MEDUSA_FLAG_HAS(pushFlags,NodePushFlags::DisableTouch));
	return layer;
}

ILayer* IScene::PushLayer(const StringRef& className, NodePushFlags pushFlags/*=NodePushFlags::None*/, const IEventArg& e/*=IEventArg::Empty*/)
{
	auto node = NodeFactory::Instance().Create(className);
	if (node == nullptr)
	{
		Log::FormatError("Cannot create scene:{}", className);
		return nullptr;
	}

	if (!node->IsA<ILayer>())
	{
		Log::FormatError("Node:{} is not a layer.", className);
		SAFE_DELETE(node);
		return nullptr;
	}
	return PushLayer((ILayer*)node, pushFlags);
}


ILayer* IScene::PushLayerEx(const StringRef& className, const FileIdRef& editorFile /*= FileIdRef::Empty*/, const FileIdRef& scriptFile /*= FileIdRef::Empty*/, NodePushFlags pushFlags/*=NodePushFlags::None*/, const IEventArg& e/*=IEventArg::Empty*/)
{
	auto node = (ILayer*)NodeFactory::Instance().Create(className, editorFile, scriptFile, e);
	if (node == nullptr)
	{
		Log::FormatError("Cannot create scene:{}", className);
		return nullptr;
	}

	if (!node->IsA<ILayer>())
	{
		Log::FormatError("Node:{} is not a layer.", className);
		SAFE_DELETE(node);
		return nullptr;
	}
	return PushLayer((ILayer*)node, pushFlags);
}

ILayer* IScene::PopLayer(NodePopFlags popFlags/*=NodePopFlags::None*/)
{
	ILayer* layer = CurrentLayer();
	Log::AssertNotNull(layer, "Layer stack should not be empty when pop.");

	if (!MEDUSA_FLAG_HAS(popFlags,NodePopFlags::ShowCurrent) || MEDUSA_FLAG_HAS(popFlags,NodePopFlags::DeleteCurrent) || MEDUSA_FLAG_HAS(popFlags,NodePopFlags::DeleteCurrentAsync))
	{
		layer->SetVisible(false);
		layer->ExitRecursively();
		layer->MutableInput().Enable(false);
	}


	if (MEDUSA_FLAG_HAS(popFlags,NodePopFlags::DeleteCurrent) || MEDUSA_FLAG_HAS(popFlags,NodePopFlags::DeleteCurrentAsync))
	{
		NodeDeleteFlags deleteFlags=NodeDeleteFlags::None;
		if (MEDUSA_FLAG_HAS(popFlags,NodePopFlags::DeleteCurrentAsync))
		{
			deleteFlags = NodeDeleteFlags::Async;
		}

		DeleteLayer(layer, deleteFlags);
		layer = nullptr;
	}
	else
	{
		RemoveChild(layer);
	}

	if (!MEDUSA_FLAG_HAS(popFlags,NodePopFlags::IgnorePrev))
	{
		ILayer* prevLayer = CurrentLayer();
		if (prevLayer != nullptr)
		{
			prevLayer->SetVisible(true);
			prevLayer->MutableInput().Enable(!MEDUSA_FLAG_HAS(popFlags,NodePopFlags::DisableTouch));
			if (!MEDUSA_FLAG_HAS(popFlags,NodePopFlags::SuppressUpdateLogic))
			{
				prevLayer->UpdateLogicRecursively();
			}
		}
	}

	if (MEDUSA_FLAG_HAS(popFlags,NodePopFlags::RestoreStatusAfterPop))
	{
		OnRestoreStatus();
	}

	return layer;
}

void IScene::PopAllLayer(NodePopFlags popFlags/*=NodePopFlags::IgnorePrevLayer*/)
{
	while (!mNodes.IsEmpty())
	{
		PopLayer(popFlags);
	}
}


ILayer* IScene::ReplaceToLayer(const StringRef& className,  NodePopFlags popFlags/*=NodePopFlags::None*/, NodePushFlags pushFlags/*=NodePushFlags::None*/, const IEventArg& e/*=IEventArg::Empty*/)
{
	PopLayer(popFlags);
	return PushLayer(className, pushFlags,e);
}

ILayer* IScene::ReplaceToLayerEx(const StringRef& className, const FileIdRef& editorFile /*= FileIdRef::Empty*/, const FileIdRef& scriptFile /*= FileIdRef::Empty*/, NodePopFlags popFlags/*=NodePopFlags::None*/, NodePushFlags pushFlags/*=NodePushFlags::None*/, const IEventArg& e/*=IEventArg::Empty*/)
{
	PopLayer(popFlags);
	return PushLayerEx(className, editorFile, scriptFile, pushFlags,e);
}

ILayer* IScene::ReplaceToLayer(ILayer* toLayer, NodePopFlags popFlags/*=NodePopFlags::None*/, NodePushFlags pushFlags/*=NodePushFlags::None*/)
{
	ILayer* prevLayer = PopLayer(popFlags);
	PushLayer(toLayer, pushFlags);
	return prevLayer;
}

void IScene::OnSaveStatus()
{
	mLayerStatusDict.Clear();
	for (auto layer : mNodes)
	{
		mLayerStatusDict.Add(layer->Name(), NodeStatus(layer->IsVisible(), layer->Input().IsEnabled()));
	}
}

void IScene::OnRestoreStatus()
{
	for (auto layer : mNodes)
	{
		NodeStatus* statusPtr = mLayerStatusDict.TryGet(layer->Name());
		if (statusPtr != nullptr)
		{
			layer->SetVisible(statusPtr->IsVisible);
			layer->MutableInput().Enable(statusPtr->InputEnabled);
			if (layer->IsVisible())
			{
				layer->EnterRecursively();
			}
			else
			{
				layer->ExitRecursively();
			}
		}
	}

	mLayerStatusDict.Clear();
}


#pragma region Update

bool IScene::UpdateScene(float dt)
{
	return UpdateRecursively(dt);
}

void IScene::AddNodeRecursivelyToVisitQueue(INode* node)
{
	RETURN_IF_FALSE(node->IsVisible());

	if (node->RenderableChangedFlag() != RenderableChangedFlags::None)
	{
		mVisitNodes.Add(node);
	}

	for (INode* child:node->MutableChildren())
	{
		AddNodeRecursivelyToVisitQueue(child);
	}

}

bool IScene::VisitScene(IVisitor < INode* >& visitor, RenderableChangedFlags& outFlag, NodeVisitFlags nodeFlag /*= NodeVisitFlags::None*/, RenderStateType renderStateFlag /*= RenderStateUpdateFlags::None*/)
{
	//VisitRecursively(visitor, outFlag, nodeFlag, renderStateFlag);
	//return true;
	if (mIsVisitQueueChanged)
	{
		mVisitNodes.Clear();
		AddNodeRecursivelyToVisitQueue(this);
		mIsVisitQueueChanged = false;
		MEDUSA_FLAG_ADD(outFlag, RenderableChangedFlags::RenderQueueChanged);
		MEDUSA_FLAG_ADD(outFlag, RenderableChangedFlags::BatchChanged);
		MEDUSA_FLAG_ADD(outFlag, RenderableChangedFlags::DataTotalChanged);
	}

	RETURN_TRUE_IF_EMPTY(mVisitNodes);
	ApplicationStatics::Instance().AddVisitNodeCount(mVisitNodes.Count());
	for(auto node: mVisitNodes)
	{
		if (node->NeedVisit())
		{
			node->VisitRecursively(visitor, outFlag, nodeFlag, renderStateFlag);
		}
	}
	mVisitNodes.Clear();

	return true;
}

void IScene::VisitRecursively(IVisitor < INode* >& visitor, RenderableChangedFlags& outFlag, NodeVisitFlags nodeFlag /*= NodeVisitFlags::None*/, RenderStateType renderStateFlag /*= RenderStateUpdateFlags::None*/)
{
	if (IsWorldMatrixDirty())
	{
		ForceSetWorldMatrix(LocalMatrix());
		MEDUSA_FLAG_ADD(nodeFlag, NodeVisitFlags::ForceUpdateWorldMatrix);
	}

	if (IsWorldColorDirty())
	{
		SetWorldColor(Color());
		MEDUSA_FLAG_ADD(nodeFlag, NodeVisitFlags::ForceUpdateWorldColor);
	}

	if (IsWorldRenderStateDirty())
	{
		MEDUSA_FLAG_ADD(renderStateFlag, mWorldRenderState.DirtyFlag());
		//renderStateFlag |= mWorldRenderState.DirtyFlag();
		ForceUpdateRenderState(renderStateFlag);

	}

	visitor.Visit(this);
	MEDUSA_FLAG_ADD(outFlag, mChangedFlag);

	//update child
	size_t size = mNodes.Count();
	RETURN_IF_ZERO(size);
	FOR_EACH_SIZE(i, size)
	{
		INode* node = mNodes[i];
		if (node->IsVisible())
		{
			node->VisitRecursively(visitor, outFlag, nodeFlag, renderStateFlag);
		}
	}

	mNeedVisit = false;
}


#pragma endregion Update


MEDUSA_END;
