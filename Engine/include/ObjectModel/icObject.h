#ifndef __IC_OBJECT_H__
#define __IC_OBJECT_H__

#include "Memory/icString.h"

namespace Ice { namespace ObjectModel { namespace Reflection
{
	class Type;
	template <class T>
	class TypeT;

	class Object
	{
	private:
		static TypeT<Object>* _type;
		static void Register();
	protected:
		~Object() {};
		Object(const Object& copy) {(void)copy;};
	public:
		static const Type* const ClassType();

	public:
		Object()
		{
			PostCreate();
		}

		virtual void Dispose() {};

		/*virtual int GetHashCode()
		{
			return (int)this;
		}*/

		virtual bool Equals(const Object* const object)
		{
			return this == object;
		}

		virtual const icString& ToString();

		const Type* const GetType();

	protected:
		virtual void PostCreate() {};
	};

} } } // Ice.ObjectModel.Reflection

#endif // #ifndef __IC_OBJECT_H__

/*
// Does a type specification contain a pointer?
template <typename icObject>
struct IsIceObject
{
static bool val = false;
};

// Specialise for yes
template <typename icObject>
struct IsIceObject<icObject*>
{
static bool val = true;
};

*/
