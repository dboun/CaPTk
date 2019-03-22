#include "CheckOpenGLVersion.h"

CheckOpenGLVersion::CheckOpenGLVersion()
{
}

#if WIN32
CheckOpenGLVersion::CheckOpenGLVersion(HINSTANCE hInstance) :
  hInstance(hInstance)
{
  pixelFormatDescriptor = { sizeof(PIXELFORMATDESCRIPTOR),
    1,
    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, // Flags
    PFD_TYPE_RGBA, // The kind of framebuffer. RGBA or palette.
    32,            // Colordepth of the framebuffer.
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    24, // Number of bits for the depthbuffer
    8,  // Number of bits for the stencilbuffer
    0,  // Number of Aux buffers in the framebuffer.
    PFD_MAIN_PLANE,
    0,
    0,
    0,
    0 };

  WNDCLASS wc = { 0 };
  wc.lpfnWndProc = DefWindowProc;
  wc.hInstance = hInstance;
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BACKGROUND);
  wc.lpszClassName = "oglversioncheck";
  wc.style = CS_OWNDC;
  if (!RegisterClass(&wc))
  {
    return;
  }
  HWND windowHandle = CreateWindow(wc.lpszClassName,
    "openglversioncheck",
    WS_OVERLAPPEDWINDOW,
    0,
    0,
    640,
    480,
    0,
    0,
    this->hInstance,
    0);

  if (windowHandle != nullptr)
  {
    HDC ourWindowHandleToDeviceContext = GetDC(windowHandle);

    int letWindowsChooseThisPixelFormat;
    letWindowsChooseThisPixelFormat =
      ChoosePixelFormat(ourWindowHandleToDeviceContext, &this->pixelFormatDescriptor);
    SetPixelFormat(ourWindowHandleToDeviceContext,
      letWindowsChooseThisPixelFormat, &this->pixelFormatDescriptor);

    HGLRC ourOpenGLRenderingContext = wglCreateContext(ourWindowHandleToDeviceContext);

    wglMakeCurrent(ourWindowHandleToDeviceContext, ourOpenGLRenderingContext);

    this->version = std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    this->renderer = std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    this->vendor = std::string(reinterpret_cast<const char*>(glGetString(GL_VENDOR)));

    // create a context
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
      reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));
    if (wglCreateContextAttribsARB)
    {
      // we believe that these later versions are all compatible with
      // OpenGL 3.2 so get a more recent context if we can.
      int attemptedVersions[] = { 4,5, 4,4, 4,3, 4,2, 4,1, 4,0, 3,3, 3,2 };
      int iContextAttribs[] =
      {
      WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
      WGL_CONTEXT_MINOR_VERSION_ARB, 2,
      WGL_CONTEXT_FLAGS_ARB, 0,
      0 // End of attributes list
      };
      HGLRC contextId = nullptr;
      for (int i = 0; i < 8 && !contextId; i++)
      {
        iContextAttribs[1] = attemptedVersions[i * 2];
        iContextAttribs[3] = attemptedVersions[i * 2 + 1];
        contextId = wglCreateContextAttribsARB(ourWindowHandleToDeviceContext, 0, iContextAttribs);
      }
      if (contextId)
      {
        wglMakeCurrent(ourWindowHandleToDeviceContext, contextId);

        glGetIntegerv(GL_MAJOR_VERSION, &this->glMajorVersion);
        glGetIntegerv(GL_MINOR_VERSION, &this->glMinorVersion);

        wglDeleteContext(contextId);
        contextId = nullptr;
      }
    }
    
    wglDeleteContext(ourOpenGLRenderingContext);
  }
}

#endif

bool CheckOpenGLVersion::hasVersion_3_2()
{
#if WIN32
  // version string should have format "4.5.0 <vendorstuff>"
  // so it is enough to parse the 0th and 2nd char
  int majorCheck = 3;
  int minorCheck = 2;

  if (this->glMajorVersion > majorCheck)
  {
    return true;
  }

  if (this->glMajorVersion == majorCheck && this->glMinorVersion >= minorCheck)
  {
    return true;
  }

  return false;
#else
  return true;
#endif
}
