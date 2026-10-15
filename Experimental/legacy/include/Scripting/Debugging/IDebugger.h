#ifndef __IC_IDEBUGGER_H__
#define __IC_IDEBUGGER_H__

namespace Ice {
	namespace Scripting {
		namespace Debugger
{

	class IDebugger
	{
	public:
		virtual ~IDebugger() {};

		virtual void OnDebugEvent(class IDebuggerEventArgs* e) = 0;

		virtual void Update(const float fDeltaTime) = 0;
	};

} } } // Ice.Scripting.Debugger

#endif // __IC_IDEBUGGER_H__
