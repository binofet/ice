# Gameplay objects and composition

This is the agreed direction, not a claim that the first Ignition scene is already a
general component runtime. That prototype currently owns primitive recipes, physics
bodies, tint and per-caller selection. The next reusable layer belongs in ICE.

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

## Acceptance cases

“Add a script to that teapot that increases specular and emissive effects as the player
approaches” resolves the caller's selection to a stable entity. Add a behavior component
with player reference, near/far distances and material ranges. The script consumes the
queried distance and returns clamped material updates for that instance. Removing the
behavior restores the declared base material policy. A missing player or retired teapot
produces a diagnostic instead of calling a dangling binding.

“When the player steps on this box, spawn a hotdog bazooka that shoots giant bouncy
hotdogs” adds a trigger and behavior to the selected box. Filter player identity, debounce
the enter event, and spawn a URI-backed weapon recipe with owner, projectile recipe,
velocity, restitution, lifetime and bounded firing rate. Imported hotdog art supplies no
weapon logic by itself. Spawned entities use the same inspectable creation operations as
the terminal and MCP clients. Tests need enter/exit/re-entry, removal during dispatch,
script failure, projectile limits and renderer replacement while the weapon remains live.

The intended natural-language layer generates these explicit recipes and behaviors. The
engine executes discoverable typed operations; matching one canned sentence is not proof
that the full scenario works.
