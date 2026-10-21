# Gameplay objects and composition

This describes proposed engine contracts. It does not claim that ICE already
implements a general component runtime; current module capabilities are documented
separately.

An entity supplies stable identity and a collection of features. Transform, renderable,
material, rigid body, trigger, script and weapon are independently attachable components.
Do not require every entity to inherit a class containing all these systems. Storage can
use typed pools where measurements justify them; a universal virtual base or a particular
third-party ECS is not a prerequisite for defining the object contract.

Entity and component identities are generation checked. Metagen describes their authored
data, references, constraints and serialization. Authored values remain separate from
derived GPU objects, physics handles and language-runtime instances. Removing a component
retires its callbacks and dependent resources at a defined boundary. An entity reference
never extends the life of a retired native pointer.

Adding or removing components is a prepared edit. Check requirements first: a rigid body
requires a transform and a supported collision recipe; a trigger requires its shape and
event routing; a script requires a supported runtime and declared bindings. Publish the
coherent change at a frame/fixed-tick boundary and return a receipt. Failures preserve the
working entity. Removing a required feature must either reject or explicitly include the
dependent removals in the same edit.

## Scripts as replaceable behavior

Humans and agents can attach a script URI plus typed inputs to an entity. Prepare a new
runtime instance separately; compile/load failures leave the previous instance active.
Bind only declared, generation checked capabilities. Native event callbacks collect owned
events, then script dispatch happens outside the physics callback. Script-produced edits
are bounded, validated and committed at the following safe boundary.

The first Lua proof uses named functions with bounded scalar input/output records. A host
can supply distance, elapsed time or trigger identity and validate the returned material
values. General query bindings, entity commands, containers and event handlers follow as
separate demonstrated extensions. A failed call may have changed private interpreter
state; disable that instance and recover through replacement rather than promise a
rollback of arbitrary language objects.

Simulation changes must not advance the authoring revision every tick. Script/component
edits have authoring receipts; simulation has ticks and events. Checkpoint undo restores a
defined snapshot, including an explicit script-state policy. It does not reverse arbitrary
file/network activity or the past behavior of another runtime.

## Conformance cases for a future component runtime

Prove identity and ownership independently of an application command vocabulary:
add/remove a component, reject unmet dependencies, retire a referenced owner and
replace a script while queued work still holds the old generation. Invalid prepared
edits must preserve the previous published state.

Script tests should exercise bounded inputs/outputs, callback failure, resource
limits and removal during dispatch. Rendering tests should retain owned data across
device replacement without reviving retired callbacks. Passing a language example
does not establish a complete entity/component system or application integration.
