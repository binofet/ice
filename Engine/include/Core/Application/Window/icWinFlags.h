#ifndef __IC_WINDOW_FLAGS_H__
#define __IC_WINDOW_FLAGS_H__

#define IC_WINDOW_NO_TITLE                (1<<0)
#define IC_WINDOW_NO_TITLE_BUTTONS        (1<<1)
#define IC_WINDOW_NO_EDGES                (1<<2)
#define IC_WINDOW_NO_RESIZE                (1<<3)

/*SW_HIDE            //    Hides the window and activates another window.
SW_MAXIMIZE        //    Maximizes the specified window.
SW_MINIMIZE        //    Minimizes the specified window and activates the next top-level window in the Z order.
SW_RESTORE        //    Activates and displays the window. If the window is minimized or maximized, the system restores it to its original size and position. An application should specify this flag when restoring a minimized window.
SW_SHOW            //    Activates the window and displays it in its current size and position. 
SW_SHOWDEFAULT    //    Sets the show state based on the SW_ value specified in the STARTUPINFO structure passed to the CreateProcess function by the program that started the application. 
SW_SHOWMAXIMIZED//Activates the window and displays it as a maximized window.
SW_SHOWMINIMIZED//Activates the window and displays it as a minimized window.
SW_SHOWMINNOACTIVE//Displays the window as a minimized window. This value is similar to SW_SHOWMINIMIZED, except the window is not activated.
SW_SHOWNA        //Displays the window in its current size and position. This value is similar to SW_SHOW, except the window is not activated.
SW_SHOWNOACTIVATE//Displays a window in its most recent size and position. This value is similar to SW_SHOWNORMAL, except the window is not actived.
SW_SHOWNORMAL*/

typedef IC_NODE{
    union{
        uchar flags;
        uchar down;
        uchar plane;
        uchar up;
    };
}:

//4 bytes
// XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX
// weight to next node
//
//   \  |  /
//    \ | /
//  ---- ----
//    / | \
//   /  |  \

#endif //__IC_WINDOW_FLAGS_H__