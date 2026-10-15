#ifndef __IC_DELEGATE_H__
#define __IC_DELEGATE_H__

#include "Core/icGlobals.h"

#if defined(_WIN64)
extern void icCallFunc();
#endif

class icIDelegate
{
public:
	virtual ~icIDelegate() {};

	//virtual void* Clone()=0;
	virtual icIDelegate* GetInvocationList(int* count) { *count = 1; return this; };

	// technically, this needs to go here
	void* DynamicInvoke(...)
	{
		register void* ptr0 = ((int**)this)[0];
#if defined(_WIN64)
		icCallFunc();
#else
#if defined(__APPLE__)
		asm("mov %al, 0");
        asm("call %ptr0");
        asm("ret");
#else
		_asm
		{
			mov eax, 0
			call ptr0
			ret
		};
#endif
#endif
        return nullptr;
	};

	bool Equals(void* obj) { return this == obj; }
	//int GetHashCode() { return reinterpret_cast<int>(this); }
};

template<class T, class RType>
class icDelegate0 : icIDelegate
{
	T* _thisPtr;
	RType(T::*_memFuncPtr)(void);
	RType(*_funcPtr)(void);
public:

	icDelegate0(RType(*funcPtr)(void))
	{
		_thisPtr = 0;
		_memFuncPtr = 0;
		_funcPtr = funcPtr;
	}

	icDelegate0(T* thisPtr, RType(T::*funcPtr)(void))
	{
		_funcPtr = 0;
		_thisPtr = thisPtr;
		_memFuncPtr = funcPtr;
	}

	RType DynamicInvoke()
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)();
		}
		return _funcPtr();
	};

	RType operator() ()
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)();
		}
		return _funcPtr();
	}
};

template<class T, class RType, class P1>
class icDelegate1 : icIDelegate
{
	T* _thisPtr;
	RType(T::*_memFuncPtr)(P1);
	RType(*_funcPtr)(P1);
public:
	icDelegate1(RType(*funcPtr)(P1))
	{
		_thisPtr = 0;
		_memFuncPtr = 0;
		_funcPtr = funcPtr;
	};

	icDelegate1(T* thisPtr, RType(T::*funcPtr)(P1))
	{
		_funcPtr = 0;
		_thisPtr = thisPtr;
		_memFuncPtr = funcPtr;
	}

	RType DynamicInvoke(P1 p1)
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)(p1);
		}
		return _funcPtr(p1);
	};

	RType operator() (P1 p1)
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)(p1);
		}
		return _funcPtr(p1);
	}
};

template<class T, class RType, class P1, class P2>
class icDelegate2 : icIDelegate
{
	T* _thisPtr;
	RType(T::*_memFuncPtr)(P1, P2);
	RType(*_funcPtr)(P1, P2);
public:
	icDelegate2(RType(*funcPtr)(P1, P2))
	{
		_thisPtr = 0;
		_memFuncPtr = 0;
		_funcPtr = funcPtr;
	};

	icDelegate2(T* thisPtr, RType(T::*funcPtr)(P1, P2))
	{
		_funcPtr = 0;
		_thisPtr = thisPtr;
		_memFuncPtr = funcPtr;
	}

	RType DynamicInvoke(P1 p1, P2 p2)
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)(p1, p2);
		}
		return _funcPtr(p1, p2);
	};

	RType operator() (P1 p1, P2 p2)
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)(p1, p2);
		}
		return _funcPtr(p1, p2);
	}
};

template<class T, class RType, class P1, class P2, class P3>
class icDelegate3 : icIDelegate
{
	T* _thisPtr;
	RType(T::*_memFuncPtr)(P1, P2, P3);
	RType(*_funcPtr)(P1, P2, P3);
public:
	icDelegate3(RType(*funcPtr)(P1, P2, P3))
	{
		_thisPtr = 0;
		_memFuncPtr = 0;
		_funcPtr = funcPtr;
	}

	icDelegate3(T* thisPtr, RType(T::*funcPtr)(P1, P2, P3))
	{
		_funcPtr = 0;
		_thisPtr = thisPtr;
		_memFuncPtr = funcPtr;
	}

	RType DynamicInvoke(P1 p1, P2 p2, P3 p3)
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)(p1, p2, p3);
		}
		return _funcPtr(p1, p2, p3);
	};

	RType operator() (P1 p1, P2 p2, P3 p3)
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)(p1, p2, p3);
		}
		return _funcPtr(p1, p2, p3);
	}
};

template<class T, class RType, class P1, class P2, class P3, class P4>
class icDelegate4 : icIDelegate
{
	T* _thisPtr;
	RType(T::*_memFuncPtr)(P1, P2, P3, P4);
	RType(*_funcPtr)(P1, P2, P3, P4);
public:
	icDelegate4(RType(*funcPtr)(P1, P2, P3, P4))
	{
		_thisPtr = 0;
		_memFuncPtr = 0;
		_funcPtr = funcPtr;
	}

	icDelegate4(T* thisPtr, RType(T::*funcPtr)(P1, P2, P3, P4))
	{
		_funcPtr = 0;
		_thisPtr = thisPtr;
		_memFuncPtr = funcPtr;
	}

	RType DynamicInvoke(P1 p1, P2 p2, P3 p3, P4 p4)
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)(p1, p2, p3, p4);
		}
		return _funcPtr(p1, p2, p3, p4);
	};

	RType operator() (P1 p1, P2 p2, P3 p3, P4 p4)
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)(p1, p2, p3, p4);
		}
		return _funcPtr(p1, p2, p3, p4);
	}
};

template<class T, class RType, class P1, class P2, class P3, class P4, class P5>
class icDelegate5 : icIDelegate
{
	T* _thisPtr;
	RType(T::*_memFuncPtr)(P1, P2, P3, P4, P5);
	RType(*_funcPtr)(P1, P2, P3, P4, P5);
public:
	icDelegate5(RType(*funcPtr)(P1, P2, P3, P4, P5))
	{
		_thisPtr = 0;
		_memFuncPtr = 0;
		_funcPtr = funcPtr;
	}

	icDelegate5(T* thisPtr, RType(T::*funcPtr)(P1, P2, P3, P4, P5))
	{
		_funcPtr = 0;
		_thisPtr = thisPtr;
		_memFuncPtr = funcPtr;
	}

	RType DynamicInvoke(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)(p1, p2, p3, p4, p5);
		}
		return _funcPtr(p1, p2, p3, p4, p5);
	};

	RType operator() (P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)
	{
		if (_thisPtr)
		{
			return (_thisPtr->*_memFuncPtr)(p1, p2, p3, p4, p5);
		}
		return _funcPtr(p1, p2, p3, p4, p5);
	}
};

#endif // __IC_DELEGATE_H__