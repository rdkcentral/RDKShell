## RDKShell

# TEST DO NOT MERGE, DUMMY CHANGE FOR TESTING BCMREF CI
RDKShell is a native component that provides application management, composition, and advanced key handling.


The RDKShell component has dependencies on Westeros, Essos, and OpenGL ES 2.0.  RDKShell uses Westeros to create a Wayland surface or display for applications to connect itself to. RDKShell provides a set of APIs for the system or applications to control the display and the positioning of the application windows on screen.

RDKShell connects to either the native windowing system or to a Wayland compositor.  It achieves this flexibility and functionality by using Essos.  By default, RDKShell will render its display to a native window (EGL).

RDKShell also provides a Thunder API.  This API allows applications to communicate with RDKShell using JSON-RPC if desired.  Alternatively, there are C++ APIs to interact with RDKShell directly in native code.

### Key Handling Sequence

RDKShell allows for applications to register for key events that they may be interested in even if they are not the focused application.  By default, all key events go to the currently focused application.  However, an application can request to intercept a key if desired.  If the key is intercepted then the focused app will not receive the key event unless if it intercepts the key as well. 
