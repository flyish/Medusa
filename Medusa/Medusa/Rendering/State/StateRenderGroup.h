// Copyright (c) 2015 fjz13. All rights reserved.
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file.
#pragma  once
#include "Rendering/IRenderGroup.h"
#include "Graphics/GraphicsTypes.h"
#include "Core/Collection/List.h"

MEDUSA_BEGIN;

class StateRenderGroup :public IRenderGroup
{
public:
	StateRenderGroup();
	virtual ~StateRenderGroup();

	RenderStateTreeLeafNode* StateNode() const { return mStateNode; }
	void SetStateNode(RenderStateTreeLeafNode* val);
public:
	virtual bool Initialize()override;
	virtual bool Uninitialize()override;

	virtual bool Add(IRenderBatch* batch)override;
	virtual void Draw(IRenderQueue& renderQueue, RenderingFlags renderingFlags = RenderingFlags::None)override;
	virtual void Print(HeapString& ioStr, uint level)override;
private:
	RenderStateTreeLeafNode* mStateNode;
	List<IRenderBatch*> mRenderBatches;
};


MEDUSA_END;