
#include "DelegateTests.h"

int TestIntValue = 42;

bool StatVoidFuncCalled = false;
bool StaticIntFuncCalled = false;
bool StaticObjectFuncCalled = false;

void StaticVoidFunc()
{
	StatVoidFuncCalled = true;
}

void StaticIntFunc(int p1)
{
	StaticIntFuncCalled = p1 == TestIntValue;
}

void StaticObjectFunc(Object* p1)
{
	StaticObjectFuncCalled = p1 != 0;
}

void DelegateTests::TestP0()
{
	T0FuncCalled = true;
}

void DelegateTests::TestP1Int(int p1)
{
	T1IntFuncCalled = p1 == TestIntValue;
}

void DelegateTests::TestP1Object(Object* p1)
{
	T1ObjectFuncCalled = p1 == this;
}

DelegateTests::~DelegateTests()
{
	if (MultiVoid)
	{
		delete MultiVoid;
	}
	if (MultiIntVoid)
	{
		delete MultiIntVoid;
	}
	if (MultiObjectVoid)
	{
		delete MultiObjectVoid;
	}
}

void DelegateTests::Reset()
{
	StatVoidFuncCalled = false;
	StaticIntFuncCalled = false;
	StaticObjectFuncCalled = false;
	T0FuncCalled = false;
	T1IntFuncCalled = false;
	T1ObjectFuncCalled = false;
}

ICRESULT DelegateTests::Init()
{
	MultiVoid = new VoidParamVoidReturnMultiDelegate(StaticVoidFunc);
	(*MultiVoid) += new VoidParamVoidReturnDelegate(this, &DelegateTests::TestP0);

	MultiIntVoid = new IntParamVoidReturnMultiDelegate(StaticIntFunc);
	(*MultiIntVoid) += new IntParamVoidReturnDelegate(this, &DelegateTests::TestP1Int);

	MultiObjectVoid = new ObjectParamVoidReturnMultiDelegate(StaticObjectFunc);
	(*MultiObjectVoid) += new ObjectParamVoidReturnDelegate(this, &DelegateTests::TestP1Object);

	return (MultiVoid && MultiIntVoid && MultiObjectVoid) ? IC_OK : IC_FAIL_GEN;
}

ICRESULT DelegateTests::UpdateTest()
{
	ICRESULT result = TestClone();
	if (ICEFAIL(result)) return result;
	result = TestInvoke();
	if (ICEFAIL(result)) return result;

	return result;
}

ICRESULT DelegateTests::TestClone()
{
	// Test void function
	//icIDelegate* clone = Void->Clone();
	//clone->DynamicInvoke();

	return IC_OK;
}

ICRESULT DelegateTests::TestInvoke()
{
	Reset();

	MultiVoid->Invoke();

	//icIDelegate* idel = MultiVoid;
	//idel->DynamicInvoke();

	(*MultiIntVoid)(TestIntValue);

	//idel = MultiIntVoid;
	//idel->DynamicInvoke(TestIntValue);

	(*MultiObjectVoid)(this);

	return StatVoidFuncCalled &&
	       StaticIntFuncCalled &&
	       StaticObjectFuncCalled &&
	       T0FuncCalled &&
	       T1IntFuncCalled &&
	       T1ObjectFuncCalled
	           ? IC_OK : IC_FAIL_GEN;
}
