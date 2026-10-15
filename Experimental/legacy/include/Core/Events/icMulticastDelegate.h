#ifndef __IC_MULTICASTDELEGATE_H__
#define __IC_MULTICASTDELEGATE_H__

#include "Core/icGlobals.h"
#include "Core/Events/icDelegate.h"


class icMulticastDelegate : public icIDelegate
{
protected:
	icIDelegate** _invocationList;
	int _invocationCount;
	int _invocationListSize;

public:
	icMulticastDelegate(int invocationListSize = 8)
	{
		_invocationCount = 1;
		_invocationListSize = invocationListSize;
		_invocationList = (icIDelegate**)malloc(sizeof(icIDelegate*)*8);
	};

	~icMulticastDelegate()
	{
		free(_invocationList);
	};

	icIDelegate* operator+= (icIDelegate* rhs)
	{
		if (_invocationCount >= _invocationListSize)
		{
			icIDelegate** tempList = (icIDelegate**)malloc(sizeof(icIDelegate*) * _invocationListSize);
			for (int i = 0; i < _invocationCount; ++i) { tempList[i] = _invocationList[i]; };
			_invocationListSize *= 2;
			_invocationList = (icIDelegate**)realloc(_invocationList, sizeof(icIDelegate*) * _invocationListSize);
			for (int i = 0; i < _invocationCount; ++i) { _invocationList[i] = tempList[i]; };
			free(tempList);
		}

		_invocationList[_invocationCount++] = rhs;
		return this;
	};

	icIDelegate* operator+= (icMulticastDelegate* rhs)
	{
		int curIndex = _invocationCount;
		_invocationCount += rhs->_invocationCount;
		while (_invocationCount >= _invocationListSize)
		{
			icIDelegate** tempList = (icIDelegate**)malloc(sizeof(icIDelegate*) * _invocationListSize);
			for (int i = 0; i < _invocationCount; ++i) { tempList[i] = _invocationList[i]; };
			_invocationListSize *= 2;
			_invocationList = (icIDelegate**)realloc(_invocationList, sizeof(icIDelegate*) * _invocationListSize);
			for (int i = 0; i < _invocationCount; ++i) { _invocationList[i] = tempList[i]; };
			free(tempList);
		}

		for (int i = 0; i < _invocationCount - curIndex; ++i)
		{
			_invocationList[i + curIndex] = rhs->_invocationList[i];
		}
		return this;
	};

	icIDelegate* operator-= (icIDelegate* rhs)
	{
		//int index = -1;
		for (int i = 0; i < _invocationCount; ++i)
		{
			if (_invocationList[i] == rhs)
			{
				for (int j = i; j < _invocationCount - 1; ++j)
				{
					_invocationList[j] = _invocationList[j + 1];
				}
				break;
			}
		};
		_invocationList[--_invocationCount] = NULL;
		return this;
	};

	//icIDelegate* operator = (icIDelegate* rhs);
	//icIDelegate* operator == (icIDelegate* rhs);
};

template<class T, class RType>
class icMulticastDelegate0 : public icMulticastDelegate
{
public:
	icMulticastDelegate0(int invocationListSize = 8) : icMulticastDelegate(invocationListSize) {};
	icMulticastDelegate0(RType (*pFunc)(void), int invocationListSize = 8) : icMulticastDelegate(invocationListSize)
	{
		_invocationList[0] = (icIDelegate*)new icDelegate0<T, RType>(pFunc);
	};
	icMulticastDelegate0(T* thisPtr, RType (T::*pFunc)(void), int invocationListSize = 8) : icMulticastDelegate(invocationListSize)
	{
		_invocationList[0] = (icIDelegate*)new icDelegate0<T, RType>(thisPtr, pFunc);
	};

	RType Invoke(void)
	{
		for (int i = 0; i < _invocationCount - 1; ++i)
		{
			((icDelegate0<T, RType>*)_invocationList[i])->DynamicInvoke();
		}

		return ((icDelegate0<T, RType>*)_invocationList[_invocationCount - 1])->DynamicInvoke();
	}

	RType operator () (void)
	{
		for (int i = 0; i < _invocationCount - 1; ++i)
		{
#if (defined __APPLE__)
			((icDelegate0<T, RType>*)_invocationList[i])->DynamicInvoke();
        }
        
		return ((icDelegate0<T, RType>*)_invocationList[_invocationCount - 1])->DynamicInvoke();
	}
#else
            ((icDelegate0<T, RType>*)_invocationList[i])->DynamicInvoke(void);
		}
        
		return ((icDelegate0<T, RType>*)_invocationList[_invocationCount - 1])->DynamicInvoke(void);
	}
#endif
};

template<class T, class RType, class P1>
class icMulticastDelegate1 : public icMulticastDelegate
{
public:
	icMulticastDelegate1(int invocationListSize = 8) : icMulticastDelegate(invocationListSize) {};
	icMulticastDelegate1(RType (*pFunc)(P1), int invocationListSize = 8) : icMulticastDelegate(invocationListSize)
	{
		_invocationList[0] = (icIDelegate*)new icDelegate1<T, RType, P1>(pFunc);
	};
	icMulticastDelegate1(T* thisPtr, RType (T::*pFunc)(P1), int invocationListSize = 8) : icMulticastDelegate(invocationListSize)
	{
		_invocationList[0] = (icIDelegate*)new icDelegate1<T, RType, P1>(thisPtr, pFunc);
	};

	RType Invoke(P1 p1)
	{
		for (int i = 0; i < _invocationCount - 1; ++i)
		{
			((icDelegate1<T, RType, P1>*)_invocationList[i])->DynamicInvoke(p1);
		}

		return ((icDelegate1<T, RType, P1>*)_invocationList[_invocationCount - 1])->DynamicInvoke(p1);
	}

	RType operator () (P1 p1)
	{
		for (int i = 0; i < _invocationCount - 1; ++i)
		{
			((icDelegate1<T, RType, P1>*)_invocationList[i])->DynamicInvoke(p1);
		}

		return ((icDelegate1<T, RType, P1>*)_invocationList[_invocationCount - 1])->DynamicInvoke(p1);
	}
};

template<class T, class RType, class P1, class P2>
class icMulticastDelegate2 : public icMulticastDelegate
{
public:
	icMulticastDelegate2(int invocationListSize = 8) : icMulticastDelegate(invocationListSize) {};
	icMulticastDelegate2(RType (*pFunc)(P1, P2), int invocationListSize = 8) : icMulticastDelegate2(invocationListSize)
	{
		_invocationList[0] = new icDelegate2<T, RType, P1, P2>(pFunc);
	};
	icMulticastDelegate2(T* thisPtr, RType (T::*pFunc)(P1, P2), int invocationListSize = 8) : icMulticastDelegate2(invocationListSize)
	{
		_invocationList[0] = new icDelegate2<T, RType, P1, P2>(thisPtr, pFunc);
	};

	RType Invoke(P1 p1, P2 p2)
	{
		for (int i = 0; i < _invocationCount - 1; ++i)
		{
			((icDelegate2<T, RType, P1, P2>*)_invocationList[i])->DynamicInvoke(p1, p2);
		}

		return ((icDelegate2<T, RType, P1, P2>*)_invocationList[_invocationCount - 1])->DynamicInvoke(p1, p2);
	}

	RType operator () (P1 p1, P2 p2)
	{
		for (int i = 0; i < _invocationCount - 1; ++i)
		{
			((icDelegate2<T, RType, P1, P2>*)_invocationList[i])->DynamicInvoke(p1, p2);
		}

		return ((icDelegate2<T, RType, P1, P2>*)_invocationList[_invocationCount - 1])->DynamicInvoke(p1, p2);
	}
};

template<class T, class RType, class P1, class P2, class P3>
class icMulticastDelegate3 : public icMulticastDelegate
{
public:
	icMulticastDelegate3(int invocationListSize = 8) : icMulticastDelegate(invocationListSize) {};
	icMulticastDelegate3(RType (*pFunc)(P1, P2, P3), int invocationListSize = 8) : icMulticastDelegate3(invocationListSize)
	{
		_invocationList[0] = new icDelegate3<T, RType, P1, P2, P3>(pFunc);
	};
	icMulticastDelegate3(T* thisPtr, RType (T::*pFunc)(P1, P2, P3), int invocationListSize = 8) : icMulticastDelegate3(invocationListSize)
	{
		_invocationList[0] = new icDelegate3<T, RType, P1, P2, P3>(thisPtr, pFunc);
	};

	RType Invoke(P1 p1, P2 p2, P3 p3)
	{
		for (int i = 0; i < _invocationCount - 1; ++i)
		{
			((icDelegate3<T, RType, P1, P2, P3>*)_invocationList[i])->DynamicInvoke(p1, p2, p3);
		}

		return ((icDelegate3<T, RType, P1, P2, P3>*)_invocationList[_invocationCount - 1])->DynamicInvoke(p1, p2, p3);
	}

	RType operator () (P1 p1, P2 p2, P3 p3)
	{
		for (int i = 0; i < _invocationCount - 1; ++i)
		{
			((icDelegate3<T, RType, P1, P2, P3>*)_invocationList[i])->DynamicInvoke(p1, p2, p3);
		}

		return ((icDelegate3<T, RType, P1, P2, P3>*)_invocationList[_invocationCount - 1])->DynamicInvoke(p1, p2, p3);
	}
};

template<class T, class RType, class P1, class P2, class P3, class P4>
class icMulticastDelegate4 : public icMulticastDelegate
{
public:
	icMulticastDelegate4(int invocationListSize = 8) : icMulticastDelegate(invocationListSize) {};
	icMulticastDelegate4(RType (*pFunc)(P1, P2, P3, P4), int invocationListSize = 8) : icMulticastDelegate4(invocationListSize)
	{
		_invocationList[0] = new icDelegate4<T, RType, P1, P2, P3, P4>(pFunc);
	};
	icMulticastDelegate4(T* thisPtr, RType (T::*pFunc)(P1, P2, P3, P4), int invocationListSize = 8) : icMulticastDelegate4(invocationListSize)
	{
		_invocationList[0] = new icDelegate4<T, RType, P1, P2, P3, P4>(thisPtr, pFunc);
	};

	RType Invoke(P1 p1, P2 p2, P3 p3, P4 p4)
	{
		for (int i = 0; i < _invocationCount - 1; ++i)
		{
			((icDelegate4<T, RType, P1, P2, P3, P4>*)_invocationList[i])->DynamicInvoke(p1, p2, p3, p4);
		}

		return ((icDelegate4<T, RType, P1, P2, P3, P4>*)_invocationList[_invocationCount - 1])->DynamicInvoke(p1, p2, p3, p4);
	}

	RType operator () (P1 p1, P2 p2, P3 p3, P4 p4)
	{
		for (int i = 0; i < _invocationCount - 1; ++i)
		{
			((icDelegate4<T, RType, P1, P2, P3, P4>*)_invocationList[i])->DynamicInvoke(p1, p2, p3, p4);
		}

		return ((icDelegate4<T, RType, P1, P2, P3, P4>*)_invocationList[_invocationCount - 1])->DynamicInvoke(p1, p2, p3, p4);
	}
};


#endif // __IC_MULTICASTDELEGATE_H__