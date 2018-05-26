OpenGL Loader
By James Dolan
-------------------------------------------

OpenGL Loader is a GLEW-like dynamic symbol loader that is based off the XML database provided by Khronos.
Also unlikely GLEW it can generate OpenGL ES bindings as well as "BigGL" bindings, and protects all of its
symbols inside namespaces so its safe to mix with code that implicitly links to OpenGL.

-------------------------------------------

To Generate:
	cd Scripts
	./genheaders.py

To Build For Android:
	cd Projects/Android
	ndk-build

To include ndk-build module:
	# insert this between 'include $(CLEAR_VARS)' and 'include $(BUILD_SHARED_LIBRARY)'
	LOCAL_STATIC_LIBRARIES += openglloader

	# insert this at the bottom of your Android.mk
	$(call import-module,1stParty/OpenGL_Loader/Projects/Android/jni)

To include GLES2 in your code:
	#include <GLES2/gl2_loader.h>
	#include <GLES2/gl2ext_loader.h>
	using namespace GLES2;
	using namespace GLES2EXT;

	// Then immediately after creating your GL context...
	if(!GLES2::LoadGLFunctions())
		FAIL("Failed to load GLES2 core entry points!");
	if(!GLES2EXT::LoadGLFunctions())
		FAIL("Failed to load GLES2 extensions!");

	// Now call your GL functions as normal...

To include GLES3 in your code:
	#include <GLES3/gl3_loader.h>
	using namespace GLES3;

	// Then immediately after creating your GL context...
	if(!GLES3::LoadGLFunctions())
		FAIL("Failed to load GLES3 core entry points!");

	// Now call your GL functions as normal...