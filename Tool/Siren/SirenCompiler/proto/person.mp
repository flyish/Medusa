//className:baseClass[classAttribute] { } propertyName[propertyAttribute]=[defaultValue];
//if has no propertyName,then it only declare a type
//if has no {}, then it's a property definition
//[classAttribute] [propertyAttribute] is optional

include(core);

enum gener
{
    female=0,
    male=1
};

class baseperson
{
    int id=0;			//player unique id
    string name="abc";		//player name
    core.address address;
};

class person:baseperson
{
    float factor=1.0;
    gener gender;
    blob data;
};

class personResponse
{
    int error;
    list<person> persons;
    dictionary<int,person> others;
};


class personMessage[Id=1]
{
    class request
    {
        int id;
    };
    
    request request[name="ddd"];
    personResponse response;
};


attribute(personMessage,Id,2);