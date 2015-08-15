
#include <string>
#include <vector>
#include <map>

#include "Memory/icMemory.h"
#include "Memory/icString.h"

#include "ObjectModel/icType.h"
#include "ObjectModel/icObject.h"

using namespace std;

namespace Ice { namespace ObjectModel { namespace Reflection
{
	static map<string, Type*> TypeNameToType;

	TypeT<Type>* Type::_type;

	void Type::Register()
	{
		_type = new TypeT<Type>("Type", "Ice.Type", sizeof(Type), const_cast<Type*>(Object::ClassType()));
	}

	const Type* const Type::GetType(const char* name)
	{
		map<string, Type*>::iterator it = TypeNameToType.find(string(name));
		if (it != TypeNameToType.end())
		{
			return (*it).second;
		}
		return 0;
	}

	const Type* const Type::GetType(const icString& name)
	{
		map<string, Type*>::iterator it = TypeNameToType.find(string(name.ToChar()));
		if (it != TypeNameToType.end())
		{
			return (*it).second;
		}
		return 0;
	}

	Type::Type(const char* szName, const char* szFullName, size_t size, Type* baseType)
	{
		_size = size;
		_name = szName;
		_fullName = szFullName;

		if (baseType)
		{
			_baseTypes.push_back(baseType);
		}

		TypeNameToType[string(szFullName)] = this;
	}

	Type::Type(const char* szName, const char* szFullName, size_t size, std::vector<Type*> baseTypes)
	{
		_size = size;
		_name = szName;
		_fullName = szFullName;

		//if (baseTypes.size() > 0)
		//{
		//	_baseTypes.insert(baseTypes.begin(), baseTypes.end());
		//}

		TypeNameToType[string(szFullName)] = this;
	}

} } } // Ice.ObjectModel.Reflection
