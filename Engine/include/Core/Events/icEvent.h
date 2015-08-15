#ifndef __IC_EVENT_H__
#define __IC_EVENT_H__

#include "Core/icGlobals.h"
#include "Core/Events/icMulticastDelegate.h"

#include "ObjectModel/icObject.h"

using namespace Ice::ObjectModel::Reflection;

class icIEventArgs
{
};


typedef icMulticastDelegate2<class DelegateTests, void, Object*, icIEventArgs*> icEventHandler;

//template<class TArgs>
//typedef icMulticastDelegate2<void, icObject*, TArgs> icEventHandler<TArgs>;

#endif // __IC_EVENT_H__