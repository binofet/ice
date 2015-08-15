#ifndef __IC_FIELD_H__
#define __IC_FIELD_H__

namespace Ice { namespace ObjectModel { namespace Reflection
{

	template <class ObjectType, class FieldType>
	class FieldDefinition : class icObject
	{
	public:
		FieldDefinition(const char* szName);

		ICRESULT GetValue(icObject* object, FieldType& value);

		ICRESULT SetValue(icObject* object, FieldType& value);

		const FieldType& getDefault() { return _defaultValue; }

	private:
		const FieldType& _defaultValue;

		icType _type;
		icString _name;
	};

} } } // Ice.ObjectModel.Reflection

#endif // __IC_FIELD_H__