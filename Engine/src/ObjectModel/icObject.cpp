#include "ObjectModel/icObject.h"
#include "ObjectModel/icType.h"
#include "Memory/icString.h"

namespace Ice { namespace ObjectModel { namespace Reflection
{
	TypeT<Object>* Object::_type = 0;

	void Object::Register()
	{
		_type = new TypeT<Object>("Object", "Ice.Object", sizeof(Object), 0);
	}

	const Type* const Object::ClassType()
	{
		if (!_type)
		{
			Object::Register();
		}
		return (Type*)_type;
	}

	const icString& Object::ToString()
	{
		return _type->Name();
	}

	const Type* const Object::GetType()
	{
		if (!_type) Object::Register();
		return (Type*)_type;
	}
	
} } } // Ice.ObjectModel.Reflection
