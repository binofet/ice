#ifndef __IC_TYPE_H__
#define __IC_TYPE_H__

#include <string>
#include <vector>
#include <map>

#include "Memory/icMemory.h"
#include "Memory/icString.h"

#include "ObjectModel/icObject.h"

using namespace std;

namespace Ice { namespace ObjectModel { namespace Reflection
{
	
	//class FieldDefinition;
	template <class T>
	class TypeT;

	class Type : public Object
	{
	private:
		static TypeT<Type>* _type;
		static void Register();
	public:
		static const Type* const ClassType();

	public:
		Type(const char* szName, const char* szFullName, size_t size, Type* baseType);
		Type(const char* szName, const char* szFullName, size_t size, vector<Type*> baseTypes);

		const icString& Name() { return _name; }

		const icString& FullName() { return _fullName; }

		const size_t Size () { return _size; }

		virtual Object* Create(icMemoryPoolI* pool = 0) const = 0;


		static const Type* const GetType(const char* name);
		static const Type* const GetType(const icString& name);

	protected:
		std::vector<Type*> _baseTypes;

		size_t _size;

		icString _name;
		icString _fullName;

		//std::map<std::string, FieldDefinition> _fields;
	};

	template <class T>
	class TypeT : public Type
	{
	public:
		TypeT<T>(const char* szName, const char* szFullName, size_t size, Type* baseType)
			: Type(szName, szFullName, size, baseType)
		{
		}

		//TypeT<T>(const char* szName, const char* szFullName, size_t size, std::vector<Type*> baseTypes);
		//	: Type(szName, szFullName, size, baseTypes)
		//{
		//}

		Object* Create(icMemoryPoolI* pool) const
		{
			Object* object = 0;
			if (pool == 0) pool = icMemory::getDefaultPool();
			if (pool != 0)
			{
				void* memory = pool->malloc(_size);
				if (memory != 0)
				{
					object = new (memory) T();
				}
			}
			return object;
		}

	private:
		TypeT(const TypeT& copy) {};
	};

	template<> Object* TypeT<Type>::Create(icMemoryPoolI* pool) const
	{
		(void)pool;
		return 0;
	}

} } } // Ice.ObjectModel.Reflection


#endif // ifndef __IC_TYPE_H__