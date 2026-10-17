project(Airhockey)


set(SOURCE_FILES
    Box2D/Collision/Shapes/b2CircleShape.cpp
    Box2D/Collision/Shapes/b2CircleShape.h
    Box2D/Collision/Shapes/b2PolygonShape.cpp
    Box2D/Collision/Shapes/b2PolygonShape.h
    Box2D/Collision/Shapes/b2Shape.cpp
    Box2D/Collision/Shapes/b2Shape.h
    Box2D/Collision/b2BroadPhase.cpp
    Box2D/Collision/b2BroadPhase.h
    Box2D/Collision/b2CollideCircle.cpp
    Box2D/Collision/b2CollidePoly.cpp
    Box2D/Collision/b2Collision.cpp
    Box2D/Collision/b2Collision.h
    Box2D/Collision/b2Distance.cpp
    Box2D/Collision/b2PairManager.cpp
    Box2D/Collision/b2PairManager.h
    Box2D/Collision/b2TimeOfImpact.cpp
    Box2D/Common/b2BlockAllocator.cpp
    Box2D/Common/b2BlockAllocator.h
    Box2D/Common/b2Math.cpp
    Box2D/Common/b2Math.h
    Box2D/Common/b2Settings.cpp
    Box2D/Common/b2Settings.h
    Box2D/Common/b2StackAllocator.cpp
    Box2D/Common/b2StackAllocator.h
    Box2D/Common/Fixed.h
    Box2D/Common/jtypes.h
    Box2D/Dynamics/Contacts/b2CircleContact.cpp
    Box2D/Dynamics/Contacts/b2CircleContact.h
    Box2D/Dynamics/Contacts/b2Contact.cpp
    Box2D/Dynamics/Contacts/b2Contact.h
    Box2D/Dynamics/Contacts/b2ContactSolver.cpp
    Box2D/Dynamics/Contacts/b2ContactSolver.h
    Box2D/Dynamics/Contacts/b2NullContact.h
    Box2D/Dynamics/Contacts/b2PolyAndCircleContact.cpp
    Box2D/Dynamics/Contacts/b2PolyAndCircleContact.h
    Box2D/Dynamics/Contacts/b2PolyContact.cpp
    Box2D/Dynamics/Contacts/b2PolyContact.h
    Box2D/Dynamics/Joints/b2DistanceJoint.cpp
    Box2D/Dynamics/Joints/b2DistanceJoint.h
    Box2D/Dynamics/Joints/b2GearJoint.cpp
    Box2D/Dynamics/Joints/b2GearJoint.h
    Box2D/Dynamics/Joints/b2Joint.cpp
    Box2D/Dynamics/Joints/b2Joint.h
    Box2D/Dynamics/Joints/b2MouseJoint.cpp
    Box2D/Dynamics/Joints/b2MouseJoint.h
    Box2D/Dynamics/Joints/b2PrismaticJoint.cpp
    Box2D/Dynamics/Joints/b2PrismaticJoint.h
    Box2D/Dynamics/Joints/b2PulleyJoint.cpp
    Box2D/Dynamics/Joints/b2PulleyJoint.h
    Box2D/Dynamics/Joints/b2RevoluteJoint.cpp
    Box2D/Dynamics/Joints/b2RevoluteJoint.h
    Box2D/Dynamics/b2Body.cpp
    Box2D/Dynamics/b2Body.h
    Box2D/Dynamics/b2ContactManager.cpp
    Box2D/Dynamics/b2ContactManager.h
    Box2D/Dynamics/b2Island.cpp
    Box2D/Dynamics/b2Island.h
    Box2D/Dynamics/b2World.cpp
    Box2D/Dynamics/b2World.h
    Box2D/Dynamics/b2WorldCallbacks.cpp
    Box2D/Dynamics/b2WorldCallbacks.h
    Box2D/Box2D.h
    source/AiInput.cpp
    source/AiInput.h
    source/airhockey.cpp
    source/airhockey.h
    source/airhockey_settings.h
    source/gamestate.cpp
    source/gamestate.h
    source/InputHandler.h
    source/main.cpp
    source/main_menu.cpp
    source/main_menu.h
    source/net_menu.cpp
    source/net_menu.h
    source/NetworkInput.cpp
    source/NetworkInput.h
    source/paddle.cpp
    source/paddle.h
    source/pause_menu.cpp
    source/pause_menu.h
    source/PlayerInput.cpp
    source/PlayerInput.h
    source/puck.cpp
    source/puck.h
    source/settings_menu.cpp
    source/settings_menu.h
    source/spinstate.cpp
    source/spinstate.h
    source/table.cpp
    source/table.h)

include_directories( ../../engine/src ../../engine/include ../../engine/3rdparty )

add_executable(Airhockey MACOSX_BUNDLE ${SOURCE_FILES})

target_link_libraries(Airhockey PRIVATE ICEngine)
target_include_directories(Airhockey PRIVATE .)
set_target_properties(Airhockey PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET Airhockey POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/gamefolder/resource" "$<TARGET_FILE_DIR:Airhockey>/resource")
add_custom_command(TARGET Airhockey POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/gamefolder/settings.ini" "$<TARGET_FILE_DIR:Airhockey>/settings.ini")

set_target_properties(Airhockey PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "com.infinitecombustion.airhockey" MACOSX_BUNDLE_BUNDLE_NAME "Airhockey" MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0" MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0")
if(WIN32)
    set_target_properties(Airhockey PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(Airhockey PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()
