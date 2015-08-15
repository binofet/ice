
#include "Core/Events/icEvent.h"

#include "ObjectModel/icObject.h"

using namespace Ice::ObjectModel::Reflection;

typedef icMulticastDelegate0<class DelegateTests, void> VoidParamVoidReturnDelegate;
typedef icMulticastDelegate1<class DelegateTests, void, int> IntParamVoidReturnDelegate;
typedef icMulticastDelegate1<class DelegateTests, void, Object*> ObjectParamVoidReturnDelegate;

typedef icMulticastDelegate0<class DelegateTests, void> VoidParamVoidReturnMultiDelegate;
typedef icMulticastDelegate1<class DelegateTests, void, int> IntParamVoidReturnMultiDelegate;
typedef icMulticastDelegate1<class DelegateTests, void, Object*> ObjectParamVoidReturnMultiDelegate;

class TestEventArgs
{
public:
	Object* _obj;
	int _id;
};

class DelegateTests : Object
{
	bool T0FuncCalled;
	bool T1IntFuncCalled;
	bool T1ObjectFuncCalled;

	icEventHandler SimpleEvent;

	//icEventHandler<TestEventArgs&>  CreatedEvent;

	VoidParamVoidReturnMultiDelegate* MultiVoid;
	IntParamVoidReturnMultiDelegate* MultiIntVoid;
	ObjectParamVoidReturnMultiDelegate* MultiObjectVoid;

	void Reset();
public:
	DelegateTests()
	{
		Reset();
	}

	~DelegateTests();

	ICRESULT Init();

	ICRESULT UpdateTest();

	ICRESULT TestClone();
	ICRESULT TestInvoke();

	void TestP0();
	void TestP1Int(int p1);
	void TestP1Object(Object* p1);

	void OnCreatedEvent(TestEventArgs* args)
	{
		//CreatedEvent(this, args);
	}

	void OnSimpleEvent(TestEventArgs* args)
	{
		SimpleEvent(this, (icIEventArgs*)args);
	}
};
