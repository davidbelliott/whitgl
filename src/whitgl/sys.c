#include <stdbool.h>
#include <stdlib.h>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <png.h>

#include <whitgl/logging.h>
#include <whitgl/math3d.h>
#include <whitgl/sys.h>

void _whitgl_sys_flush_tex_iaabb();

whitgl_bool _shouldClose;
whitgl_ivec _window_size;
GLFWwindow* _window;
whitgl_bool _windowFocused;

typedef struct
{
	int id;
	GLuint gluint;
	whitgl_ivec size;
} whitgl_image;
#define WHITGL_IMAGE_MAX (64)
whitgl_image images[WHITGL_IMAGE_MAX];
int num_images;

const char* _vertex_src = "\
#version 150\
\n\
\
in vec2 position;\
in vec2 texturepos;\
out vec2 Texturepos;\
uniform mat4 sLocalToProjMatrix;\
void main()\
{\
	gl_Position = sLocalToProjMatrix * vec4( position, 0.0, 1.0 );\
	Texturepos = texturepos;\
}\
";

const char* _fragment_src = "\
#version 150\
\n\
in vec2 Texturepos;\
out vec4 outColor;\
uniform sampler2D tex;\
void main()\
{\
	outColor = texture( tex, Texturepos );\
}\
";

const char* _flat_src = "\
#version 150\
\n\
uniform vec4 sColor;\
out vec4 outColor;\
void main()\
{\
	outColor = sColor;\
}\
";

typedef struct
{
	GLuint program;
	float uniforms[WHITGL_MAX_SHADER_UNIFORMS];
	whitgl_sys_color colors[WHITGL_MAX_SHADER_COLORS];
	whitgl_int images[WHITGL_MAX_SHADER_IMAGES];
	whitgl_shader shader;
} whitgl_shader_data;

typedef struct
{
	bool do_next;
	char file[512];
} whitgl_frame_capture;
static const whitgl_frame_capture whitgl_frame_capture_zero = {false, {'\0'}};

GLuint vbo;
whitgl_shader_data shaders[WHITGL_SHADER_MAX];
GLuint frameBuffer;
GLuint intermediateTexture;
whitgl_frame_capture capture;

void _whitgl_check_gl_error(const char* stmt, const char *file, int line)
{
	GLenum err = glGetError();
	if(!err) return;
	const char* error = "UNKNOWN GL ERROR!";
	switch(err)
	{
		case GL_NO_ERROR: error = "GL_NO_ERROR"; break;
		case GL_INVALID_ENUM: error = "GL_INVALID_ENUM"; break;
		case GL_INVALID_VALUE: error = "GL_INVALID_VALUE"; break;
		case GL_INVALID_OPERATION: error = "GL_INVALID_OPERATION"; break;
		case GL_INVALID_FRAMEBUFFER_OPERATION: error = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
		case GL_OUT_OF_MEMORY: error = "GL_OUT_OF_MEMORY"; break;
		case GL_STACK_UNDERFLOW: error = "GL_STACK_UNDERFLOW"; break;
		case GL_STACK_OVERFLOW: error = "GL_STACK_OVERFLOW"; break;
	}
	whitgl_logit(file, line, "%s in %s", error, stmt);
}

#define GL_CHECK(stmt) do { \
		stmt; \
		_whitgl_check_gl_error(#stmt, __FILE__, __LINE__); \
	} while (0)


void _whitgl_sys_close_callback(GLFWwindow*);
void _whitgl_sys_glfw_error_callback(int code, const char* error);
void _whitgl_sys_window_focus_callback(GLFWwindow *window, int focused);

whitgl_sys_setup _setup;

void whitgl_sys_set_clear_color(whitgl_sys_color col)
{
	float r = (float)col.r/255.0;
	float g = (float)col.g/255.0;
	float b = (float)col.b/255.0;
	float a = (float)col.a/255.0;
	glClearColor(r, g, b, a);
}

bool whitgl_change_shader(whitgl_shader_slot type, whitgl_shader shader)
{
	if(type >= WHITGL_SHADER_MAX)
	{
		WHITGL_PANIC("Invalid shader type %d", type);
		return false;
	}
	shaders[type].shader = shader;

	if(shader.vertex_src == NULL)
		shader.vertex_src = _vertex_src;
	if(shader.fragment_src == NULL)
		shader.fragment_src = _fragment_src;

	if(glIsProgram(shaders[type].program))
		glDeleteProgram(shaders[type].program);

	GLuint vertexShader = glCreateShader( GL_VERTEX_SHADER );
	glShaderSource( vertexShader, 1, &shader.vertex_src, NULL );
	glCompileShader( vertexShader );
	GLint status;
	glGetShaderiv( vertexShader, GL_COMPILE_STATUS, &status );
	if(status != GL_TRUE)
	{
		char buffer[512];
		glGetShaderInfoLog( vertexShader, 512, NULL, buffer );
		WHITGL_PANIC(buffer);
		return false;
	}

	GLuint fragmentShader = glCreateShader( GL_FRAGMENT_SHADER );
	glShaderSource( fragmentShader, 1, &shader.fragment_src, NULL );
	glCompileShader( fragmentShader );

	glGetShaderiv( fragmentShader, GL_COMPILE_STATUS, &status );
	if(status != GL_TRUE)
	{
		char buffer[512];
		glGetShaderInfoLog( fragmentShader, 512, NULL, buffer );
		WHITGL_PANIC(buffer);
		return false;
	}

	shaders[type].program = glCreateProgram();
	glAttachShader( shaders[type].program, vertexShader );
	glAttachShader( shaders[type].program, fragmentShader );
	glBindFragDataLocation( shaders[type].program, 0, "outColor" );
	glLinkProgram( shaders[type].program );

	return true;
}

void whitgl_set_shader_uniform(whitgl_shader_slot type, int uniform, float value)
{
	if(type >= WHITGL_SHADER_MAX)
	{
		WHITGL_PANIC("Invalid shader type %d", type);
		return;
	}
	if(uniform < 0 || uniform >= WHITGL_MAX_SHADER_UNIFORMS || uniform >= shaders[type].shader.num_uniforms)
	{
		WHITGL_PANIC("Invalid shader uniform %d", uniform);
		return;
	}
	if(shaders[type].uniforms[uniform] != value)
		_whitgl_sys_flush_tex_iaabb();
	shaders[type].uniforms[uniform] = value;
}
void whitgl_set_shader_color(whitgl_shader_slot type, whitgl_int color, whitgl_sys_color value)
{
	if(type >= WHITGL_SHADER_MAX)
	{
		WHITGL_PANIC("Invalid shader type %d", type);
		return;
	}
	if(color < 0 || color >= WHITGL_MAX_SHADER_COLORS || color >= shaders[type].shader.num_colors)
	{
		WHITGL_PANIC("Invalid shader uniform %d", color);
		return;
	}
	if(shaders[type].colors[color].r != value.r ||
	   shaders[type].colors[color].g != value.g ||
	   shaders[type].colors[color].b != value.b ||
	   shaders[type].colors[color].a != value.a)
		_whitgl_sys_flush_tex_iaabb();
	shaders[type].colors[color] = value;
}
void whitgl_set_shader_image(whitgl_shader_slot type, whitgl_int image, whitgl_int index)
{
	if(type >= WHITGL_SHADER_MAX)
	{
		WHITGL_PANIC("Invalid shader type %d", type);
		return;
	}
	if(image < 0 || image >= WHITGL_MAX_SHADER_IMAGES || image >= shaders[type].shader.num_images)
	{
		WHITGL_PANIC("Invalid shader uniform %d", image);
		return;
	}
	if(shaders[type].images[image] != index)
		_whitgl_sys_flush_tex_iaabb();
	shaders[type].images[image] = index;
}

bool whitgl_sys_init(whitgl_sys_setup* setup)
{
	bool result;
	_shouldClose = false;

	WHITGL_LOG("Initialize GLFW");

	glfwSetErrorCallback(&_whitgl_sys_glfw_error_callback);
	result = glfwInit();
	if(!result)
	{
		WHITGL_PANIC("Failed to initialize GLFW");
		exit( EXIT_FAILURE );
	}

	WHITGL_LOG("Setting GLFW window hints");
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	_windowFocused = setup->start_focused;
	if(!_windowFocused)
		glfwWindowHint(GLFW_FOCUSED, GL_FALSE);
	// glfwWindowHint( GLFW_RESIZABLE, GL_FALSE );

	const GLFWvidmode * mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

	// attempt to ensure that no mode changing takes place
	glfwWindowHint(GLFW_RED_BITS, mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

	if(setup->fullscreen)
	{
		WHITGL_LOG("Opening fullscreen w%d h%d", mode->width, mode->height);
		_window = glfwCreateWindow(mode->width, mode->height, setup->name, glfwGetPrimaryMonitor(), NULL);
	} else
	{
		whitgl_ivec window_size;
		window_size.x = setup->size.x*setup->pixel_size;
		window_size.y = setup->size.y*setup->pixel_size;
		WHITGL_LOG("Opening windowed w%d h%d", window_size.x, window_size.y);
		_window = glfwCreateWindow(window_size.x, window_size.y, setup->name, NULL, NULL);
	}

	if(!_window)
		WHITGL_LOG("Failed to create window.");

	 glfwSetWindowFocusCallback (_window, _whitgl_sys_window_focus_callback);


	// Retrieve actual size of window in pixels, don't make an assumption
	// (because of retina screens etc.
	int w, h;
	glfwGetFramebufferSize(_window, &w, &h);
	whitgl_ivec screen_size = {w, h};

	bool searching = true;
	if(!setup->exact_size)
	{
		setup->pixel_size = screen_size.x/setup->size.x;
		whitgl_ivec new_size = setup->size;
		while(searching)
		{
			new_size.x = screen_size.x/setup->pixel_size;
			new_size.y = screen_size.y/setup->pixel_size;
			searching = false;
			if(new_size.x < setup->size.x) searching = true;
			if(new_size.y < setup->size.y) searching = true;
			if(setup->pixel_size == 1) searching = false;
			if(searching) setup->pixel_size--;
		}
		if(setup->over_render)
			setup->pixel_size++;
		setup->size = whitgl_ivec_divide(screen_size, whitgl_ivec_val(setup->pixel_size));
	}
	if(!_window)
	{
		WHITGL_PANIC("Failed to open GLFW window");
		exit( EXIT_FAILURE );
	}
	glfwMakeContextCurrent(_window);

	WHITGL_LOG("Initiating glew");
	glewExperimental = GL_TRUE;
	glewInit();
	glGetError(); // Ignore any glGetError in glewInit, nothing to panic about, see https://www.opengl.org/wiki/OpenGL_Loading_Library

	WHITGL_LOG("Generating vbo");
	GL_CHECK( glGenBuffers( 1, &vbo ) ); // Generate 1 buffer

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	WHITGL_LOG("Loading shaders");
	whitgl_shader flat_shader = whitgl_shader_zero;
	flat_shader.fragment_src = _flat_src;
	if(!whitgl_change_shader( WHITGL_SHADER_FLAT, flat_shader))
		return false;
	whitgl_shader texture_shader = whitgl_shader_zero;
	if(!whitgl_change_shader( WHITGL_SHADER_TEXTURE, texture_shader))
		return false;
	if(!whitgl_change_shader( WHITGL_SHADER_POST, texture_shader))
		return false;

	WHITGL_LOG("Creating framebuffer");
	// The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
	GL_CHECK( glGenFramebuffers(1, &frameBuffer) );
	GL_CHECK( glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer) );
	GL_CHECK( glGenTextures(1, &intermediateTexture) );
	GL_CHECK( glBindTexture(GL_TEXTURE_2D, intermediateTexture) );
	WHITGL_LOG("Creating framebuffer glTexImage2D");
	GL_CHECK( glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, setup->size.x, setup->size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0) );
	GL_CHECK( glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
	GL_CHECK( glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );
	GL_CHECK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST) );
	GL_CHECK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST) );
	GL_CHECK( glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, intermediateTexture, 0) );
	GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
	GL_CHECK( glDrawBuffers(1, drawBuffers) ); // "1" is the size of drawBuffers
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		WHITGL_LOG("Problem setting up intermediate render target");
	GL_CHECK( glBindFramebuffer(GL_FRAMEBUFFER, 0) );

	if(setup->vsync)
	{
		WHITGL_LOG("Enabling vsync");
		GL_CHECK( glfwSwapInterval(1) ); // some cards still wont vsync!
	} else
	{
		WHITGL_LOG("Disabling vsync");
		GL_CHECK( glfwSwapInterval(0) );
	}

	WHITGL_LOG("Setting close callback");
	glfwSetWindowCloseCallback(_window, _whitgl_sys_close_callback);

	// Set mouse cursor mode
	WHITGL_LOG("Disable mouse cursor");
	whitgl_int glfw_mode;
	switch(setup->cursor)
	{
		case CURSOR_HIDE: glfw_mode = GLFW_CURSOR_HIDDEN; break;
		case CURSOR_DISABLE: glfw_mode = GLFW_CURSOR_DISABLED; break;
		case CURSOR_SHOW:
		default:
			glfw_mode = GLFW_CURSOR_NORMAL; break;
	}
	glfwSetInputMode (_window, GLFW_CURSOR, glfw_mode);

	int i;
	for(i=0; i<WHITGL_IMAGE_MAX; i++)
		images[i].id = -1;
	num_images = 0;

	_setup = *setup;

	capture = whitgl_frame_capture_zero;

	WHITGL_LOG("Sys initiated");

	return true;
}

void _whitgl_sys_close_callback(GLFWwindow* window)
{
	(void)window;
	_shouldClose = true;
}

void _whitgl_sys_glfw_error_callback(int code, const char* error)
{
	WHITGL_PANIC("glfw error %d '%s'", code, error);
}

void _whitgl_sys_window_focus_callback(GLFWwindow *window, int focused)
{
	(void)window;
	_windowFocused = focused;
}

bool whitgl_sys_should_close()
{
	return _shouldClose;
}

void whitgl_sys_close()
{
	glfwTerminate();
}

double whitgl_sys_getTime()
{
	return glfwGetTime();
}

void whitgl_sys_draw_init()
{
	int w, h;
	glfwGetFramebufferSize(_window, &w, &h);
	_window_size.x = w;
	_window_size.y = h;

	GL_CHECK( glEnable(GL_BLEND) );
	GL_CHECK( glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) );
	GL_CHECK( glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer) );
	GL_CHECK( glViewport( 0, 0, _setup.size.x, _setup.size.y ) );
	if(_setup.clear_buffer)
		GL_CHECK( glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) );
}

void _whitgl_populate_vertices(float* vertices, whitgl_iaabb s, whitgl_iaabb d, whitgl_ivec image_size)
{
	// cpu optimisation for "whitgl_faabb sf = whitgl_faabb_divide(whitgl_iaabb_to_faabb(s), whitgl_ivec_to_fvec(image_size));"
	whitgl_faabb sf = {{((float)s.a.x)/((float)image_size.x),((float)s.a.y)/((float)image_size.y)},
                       {((float)s.b.x)/((float)image_size.x),((float)s.b.y)/((float)image_size.y)}};
	vertices[ 0] = d.a.x; vertices[ 1] = d.b.y; vertices[ 2] = sf.a.x; vertices[ 3] = sf.b.y;
	vertices[ 4] = d.b.x; vertices[ 5] = d.a.y; vertices[ 6] = sf.b.x; vertices[ 7] = sf.a.y;
	vertices[ 8] = d.a.x; vertices[ 9] = d.a.y; vertices[10] = sf.a.x; vertices[11] = sf.a.y;

	vertices[12] = d.a.x; vertices[13] = d.b.y; vertices[14] = sf.a.x; vertices[15] = sf.b.y;
	vertices[16] = d.b.x; vertices[17] = d.b.y; vertices[18] = sf.b.x; vertices[19] = sf.b.y;
	vertices[20] = d.b.x; vertices[21] = d.a.y; vertices[22] = sf.b.x; vertices[23] = sf.a.y;
}

void _whitgl_sys_orthographic(GLuint program, float left, float right, float top, float bottom)
{
	whitgl_fmat m = whitgl_fmat_orthographic(left, right, top, bottom, 0, 100);
	glUniformMatrix4fv( glGetUniformLocation( program, "sLocalToProjMatrix"), 1, GL_FALSE, m.mat);
	GL_CHECK( return );
}

void _whitgl_load_uniforms(whitgl_shader_slot slot)
{
	int i;
	for(i=0; i<shaders[slot].shader.num_uniforms; i++)
		glUniform1f( glGetUniformLocation( shaders[slot].program, shaders[slot].shader.uniforms[i]), shaders[slot].uniforms[i]);
	for(i=0; i<shaders[slot].shader.num_colors; i++)
	{
		whitgl_sys_color c = shaders[slot].colors[i];
		float r = ((float)c.r)/255.0;
		float g = ((float)c.g)/255.0;
		float b = ((float)c.b)/255.0;
		float a = ((float)c.a)/255.0;
		glUniform4f( glGetUniformLocation( shaders[slot].program, shaders[slot].shader.colors[i]), r, g, b, a);
	}
	for(i=0; i<shaders[slot].shader.num_images; i++)
	{
		GLint location = glGetUniformLocation(shaders[slot].program, shaders[slot].shader.images[i]);
		glUniform1i(location, i+1);
		whitgl_int image = shaders[slot].images[i];
		glActiveTexture(GL_TEXTURE0 + 1 + i);
		glBindTexture(GL_TEXTURE_2D, images[image].gluint);
	}
	GL_CHECK( return );
}

void whitgl_sys_draw_finish()
{
	_whitgl_sys_flush_tex_iaabb();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	GL_CHECK( glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) );
	GL_CHECK( glViewport( 0, 0, _window_size.x, _window_size.y ) );
	GL_CHECK( glActiveTexture( GL_TEXTURE0 ) );
	GL_CHECK( glBindTexture( GL_TEXTURE_2D, intermediateTexture ) );

	float vertices[6*4];
	whitgl_iaabb src = whitgl_iaabb_zero;
	whitgl_iaabb dest = whitgl_iaabb_zero;
	src.b.x = _setup.size.x;
	src.a.y = _setup.size.y;
	if(_setup.exact_size)
	{
		dest.b = _window_size;
	}
	else
	{
		dest.a.x = (_window_size.x-_setup.size.x*_setup.pixel_size)/2;
		dest.a.y = (_window_size.y-_setup.size.y*_setup.pixel_size)/2;
		dest.b.x = dest.a.x+_setup.size.x*_setup.pixel_size;
		dest.b.y = dest.a.y+_setup.size.y*_setup.pixel_size;
	}

	_whitgl_populate_vertices(vertices, src, dest, _setup.size);
	GL_CHECK( glBindBuffer( GL_ARRAY_BUFFER, vbo ) );
	GL_CHECK( glBufferData( GL_ARRAY_BUFFER, sizeof( vertices ), vertices, GL_DYNAMIC_DRAW ) );

	GLuint shaderProgram = shaders[WHITGL_SHADER_POST].program;
	GL_CHECK( glUseProgram( shaderProgram ) );
	GL_CHECK( glUniform1i( glGetUniformLocation( shaderProgram, "tex" ), 0 ) );
	_whitgl_load_uniforms(WHITGL_SHADER_POST);
	_whitgl_sys_orthographic(shaderProgram, 0, _window_size.x, 0, _window_size.y);

	#define BUFFER_OFFSET(i) ((void*)(i))
	GLint posAttrib = glGetAttribLocation( shaderProgram, "position" );
	GL_CHECK( glVertexAttribPointer( posAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0 ) );
	GL_CHECK( glEnableVertexAttribArray( posAttrib ) );

	GLint texturePosAttrib = glGetAttribLocation( shaderProgram, "texturepos" );
	GL_CHECK( glVertexAttribPointer( texturePosAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), BUFFER_OFFSET(sizeof(float)*2) ) );
	GL_CHECK( glEnableVertexAttribArray( texturePosAttrib ) );

	GL_CHECK( glDrawArrays( GL_TRIANGLES, 0, 6 ) );

	if(capture.do_next)
	{
		unsigned char* buffer = malloc(_window_size.x*_window_size.y*4);
		glReadPixels(0, 0, _window_size.x, _window_size.y, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
		whitgl_sys_save_png(capture.file, _window_size.x, _window_size.y, buffer);
		free(buffer);
		capture.do_next = false;
	}

	glfwSwapBuffers(_window);
	glfwPollEvents();
	GL_CHECK( glDisable(GL_BLEND) );
}

void whitgl_sys_draw_iaabb(whitgl_iaabb rect, whitgl_sys_color col)
{
	_whitgl_sys_flush_tex_iaabb();
	float vertices[6*4];
	_whitgl_populate_vertices(vertices, whitgl_iaabb_zero, rect, whitgl_ivec_zero);

	GL_CHECK( glBindBuffer( GL_ARRAY_BUFFER, vbo ) );
	GL_CHECK( glBufferData( GL_ARRAY_BUFFER, sizeof( vertices ), vertices, GL_DYNAMIC_DRAW ) );

	GLuint shaderProgram = shaders[WHITGL_SHADER_FLAT].program;
	GL_CHECK( glUseProgram( shaderProgram ) );
	GL_CHECK( glUniform4f( glGetUniformLocation( shaderProgram, "sColor" ), (float)col.r/255.0, (float)col.g/255.0, (float)col.b/255.0, (float)col.a/255.0 ) );
	_whitgl_load_uniforms(WHITGL_SHADER_FLAT);
	_whitgl_sys_orthographic(shaderProgram, 0, _setup.size.x, 0, _setup.size.y);

	#define BUFFER_OFFSET(i) ((void*)(i))
	GLint posAttrib = glGetAttribLocation( shaderProgram, "position" );
	GL_CHECK( glVertexAttribPointer( posAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0 ) );
	GL_CHECK( glEnableVertexAttribArray( posAttrib ) );

	GL_CHECK( glDrawArrays( GL_TRIANGLES, 0, 6 ) );
}

void whitgl_sys_draw_hollow_iaabb(whitgl_iaabb rect, whitgl_int width, whitgl_sys_color col)
{
	whitgl_iaabb n = {rect.a, {rect.b.x, rect.a.y+width}};
	whitgl_iaabb e = {{rect.b.x-width, rect.a.y}, rect.b};
	whitgl_iaabb s = {{rect.a.x, rect.b.y-width}, rect.b};
	whitgl_iaabb w = {rect.a, {rect.a.x+width, rect.b.y}};
	whitgl_sys_draw_iaabb(n, col);
	whitgl_sys_draw_iaabb(e, col);
	whitgl_sys_draw_iaabb(s, col);
	whitgl_sys_draw_iaabb(w, col);
}
void whitgl_sys_draw_line(whitgl_iaabb l, whitgl_sys_color col)
{
	_whitgl_sys_flush_tex_iaabb();
	float vertices[2*2];
	vertices[0] = l.a.x; vertices[1] = l.a.y; vertices[2] = l.b.x; vertices[3] = l.b.y;

	GL_CHECK( glBindBuffer( GL_ARRAY_BUFFER, vbo ) );
	GL_CHECK( glBufferData( GL_ARRAY_BUFFER, sizeof( vertices ), vertices, GL_DYNAMIC_DRAW ) );

	GLuint shaderProgram = shaders[WHITGL_SHADER_FLAT].program;
	GL_CHECK( glUseProgram( shaderProgram ) );
	GL_CHECK( glUniform4f( glGetUniformLocation( shaderProgram, "sColor" ), (float)col.r/255.0, (float)col.g/255.0, (float)col.b/255.0, (float)col.a/255.0 ) );
	_whitgl_load_uniforms(WHITGL_SHADER_FLAT);
	_whitgl_sys_orthographic(shaderProgram, 0, _setup.size.x, 0, _setup.size.y);

	#define BUFFER_OFFSET(i) ((void*)(i))
	GLint posAttrib = glGetAttribLocation( shaderProgram, "position" );
	GL_CHECK( glVertexAttribPointer( posAttrib, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), 0 ) );
	GL_CHECK( glEnableVertexAttribArray( posAttrib ) );

	GL_CHECK( glDrawArrays( GL_LINES, 0, 2 ) );
}
void whitgl_sys_draw_fcircle(whitgl_fcircle c, whitgl_sys_color col, int tris)
{
	_whitgl_sys_flush_tex_iaabb();
	int num_vertices = tris*3*2;
	whitgl_fvec scale = {c.size, c.size};
	float *vertices = malloc(sizeof(float)*num_vertices);
	int i;
	for(i=0; i<tris; i++)
	{
		whitgl_float dir;
		whitgl_fvec off;
		int vertex_offset = 6*i;
		vertices[vertex_offset+0] = c.pos.x; vertices[vertex_offset+1] = c.pos.y;

		dir = ((whitgl_float)i)/tris * whitgl_pi * 2 ;
		off = whitgl_fvec_scale(whitgl_angle_to_fvec(dir), scale);
		vertices[vertex_offset+2] = c.pos.x+off.x; vertices[vertex_offset+3] = c.pos.y+off.y;

		dir = ((whitgl_float)(i+1))/tris * whitgl_pi * 2;
		off = whitgl_fvec_scale(whitgl_angle_to_fvec(dir), scale);
		vertices[vertex_offset+4] = c.pos.x+off.x; vertices[vertex_offset+5] = c.pos.y+off.y;
	}

	GL_CHECK( glBindBuffer( GL_ARRAY_BUFFER, vbo ) );
	GL_CHECK( glBufferData( GL_ARRAY_BUFFER, sizeof(float)*num_vertices , vertices, GL_DYNAMIC_DRAW ) );

	GLuint shaderProgram = shaders[WHITGL_SHADER_FLAT].program;
	GL_CHECK( glUseProgram( shaderProgram ) );
	GL_CHECK( glUniform4f( glGetUniformLocation( shaderProgram, "sColor" ), (float)col.r/255.0, (float)col.g/255.0, (float)col.b/255.0, (float)col.a/255.0 ) );
	_whitgl_load_uniforms(WHITGL_SHADER_FLAT);
	_whitgl_sys_orthographic(shaderProgram, 0, _setup.size.x, 0, _setup.size.y);

	#define BUFFER_OFFSET(i) ((void*)(i))
	GLint posAttrib = glGetAttribLocation( shaderProgram, "position" );
	GL_CHECK( glVertexAttribPointer( posAttrib, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), 0 ) );
	GL_CHECK( glEnableVertexAttribArray( posAttrib ) );

	GL_CHECK( glDrawArrays( GL_TRIANGLES, 0, tris*3 ) );
	free(vertices);
}

whitgl_int buffer_curindex = -1;
static const whitgl_int buffer_num_quads = 128;
float buffer_vertices[128*6*4];
whitgl_int buffer_index = 0;
void _whitgl_sys_flush_tex_iaabb()
{
	if(buffer_index == 0 || buffer_curindex == -1)
		return;
	GL_CHECK( glActiveTexture( GL_TEXTURE0 ) );
	GL_CHECK( glBindTexture( GL_TEXTURE_2D, images[buffer_curindex].gluint ) );

	GL_CHECK( glBindBuffer( GL_ARRAY_BUFFER, vbo ) );
	GL_CHECK( glBufferData( GL_ARRAY_BUFFER, sizeof(float)*buffer_index*6*4, buffer_vertices, GL_DYNAMIC_DRAW ) );

	GLuint shaderProgram = shaders[WHITGL_SHADER_TEXTURE].program;
	GL_CHECK( glUseProgram( shaderProgram ) );
	GL_CHECK( glUniform1i( glGetUniformLocation( shaderProgram, "tex" ), 0 ) );
	_whitgl_load_uniforms(WHITGL_SHADER_TEXTURE);
	_whitgl_sys_orthographic(shaderProgram, 0, _setup.size.x, 0, _setup.size.y);

	#define BUFFER_OFFSET(i) ((void*)(i))
	GLint posAttrib = glGetAttribLocation( shaderProgram, "position" );
	GL_CHECK( glVertexAttribPointer( posAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0 ) );
	GL_CHECK( glEnableVertexAttribArray( posAttrib ) );

	GLint texturePosAttrib = glGetAttribLocation( shaderProgram, "texturepos" );
	GL_CHECK( glVertexAttribPointer( texturePosAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), BUFFER_OFFSET(sizeof(float)*2) ) );
	GL_CHECK( glEnableVertexAttribArray( texturePosAttrib ) );

	GL_CHECK( glDrawArrays( GL_TRIANGLES, 0, buffer_index*6 ) );
	buffer_curindex = -1;
	buffer_index = 0;
}
void whitgl_sys_draw_tex_iaabb(int id, whitgl_iaabb src, whitgl_iaabb dest)
{
	int index = -1;
	int i;
	for(i=0; i<num_images; i++)
	{
		if(images[i].id == id)
		{
			index = i;
			break;
		}
	}
	if(index == -1)
	{
		WHITGL_PANIC("ERR Cannot find image %d", id);
		return;
	}
	if(buffer_curindex != index || buffer_index+1 >= buffer_num_quads)
	{
		if(buffer_curindex != -1)
			_whitgl_sys_flush_tex_iaabb();
		buffer_curindex = index;
	}
	_whitgl_populate_vertices(&buffer_vertices[buffer_index*6*4], src, dest, images[index].size);
	buffer_index++;
}

void whitgl_sys_draw_sprite(whitgl_sprite sprite, whitgl_ivec frame, whitgl_ivec pos)
{
	whitgl_sys_draw_sprite_sized(sprite, frame, pos, sprite.size);
}

void whitgl_sys_draw_sprite_sized(whitgl_sprite sprite, whitgl_ivec frame, whitgl_ivec pos, whitgl_ivec dest_size)
{
	whitgl_iaabb src = whitgl_iaabb_zero;
	whitgl_ivec offset = whitgl_ivec_scale(sprite.size, frame);
	src.a = whitgl_ivec_add(sprite.top_left, offset);
	src.b = whitgl_ivec_add(src.a, sprite.size);
	whitgl_iaabb dest = whitgl_iaabb_zero;
	dest.a = pos;
	dest.b = whitgl_ivec_add(dest.a, dest_size);
	whitgl_sys_draw_tex_iaabb(sprite.image, src, dest);
}

void whitgl_sys_draw_text(whitgl_sprite sprite, const char* string, whitgl_ivec pos)
{
	whitgl_ivec draw_pos = pos;
	while(*string)
	{
		int index = -1;
		if(*string >= 'a' && *string <= 'z')
			index = *string-'a';
		if(*string >= 'A' && *string <= 'Z')
			index = *string-'A';
		if(*string >= '0' && *string <= '9')
			index = *string-'0'+26;
		if(*string == ',')
			index = 36;
		if(*string == '.')
			index = 37;
		if(*string == ':')
			index = 38;
		if(*string == '$')
			index = 39;
		if(*string == '!')
			index = 40;
		if(*string == '\'')
			index = 41;
		if(*string == '?')
			index = 42;
		if(*string == '-')
			index = 43;
		if(*string == '<')
			index = 44;
		if(*string == '>')
			index = 45;
		if(*string == ' ')
			draw_pos.x += sprite.size.x;
		if(index != -1)
		{
			whitgl_ivec frame = {index%6, index/6};
			whitgl_sys_draw_sprite(sprite, frame, draw_pos);
			draw_pos.x += sprite.size.x;
		}
		string++;
	}
}

bool whitgl_sys_load_png(const char *name, whitgl_int *width, whitgl_int *height, unsigned char **data)
{
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned int sig_read = 0;
	int color_type, interlace_type;
	FILE *fp;

	if ((fp = fopen(name, "rb")) == NULL)
		return false;

	/* Create and initialize the png_struct
	 * with the desired error handler
	 * functions.  If you want to use the
	 * default stderr and longjump method,
	 * you can supply NULL for the last
	 * three parameters.  We also supply the
	 * the compiler header file version, so
	 * that we know if the application
	 * was compiled with a compatible version
	 * of the library.  REQUIRED
	 */
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
									 NULL, NULL, NULL);

	if (png_ptr == NULL) {
		fclose(fp);
		return false;
	}

	/* Allocate/initialize the memory
	 * for image information.  REQUIRED. */
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		fclose(fp);
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return false;
	}

	/* Set error handling if you are
	 * using the setjmp/longjmp method
	 * (this is the normal method of
	 * doing things with libpng).
	 * REQUIRED unless you  set up
	 * your own error handlers in
	 * the png_create_read_struct()
	 * earlier.
	 */
	if (setjmp(png_jmpbuf(png_ptr))) {
		/* Free all of the memory associated
		 * with the png_ptr and info_ptr */
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		/* If we get here, we had a
		 * problem reading the file */
		return false;
	}

	/* Set up the output control if
	 * you are using standard C streams */
	png_init_io(png_ptr, fp);

	/* If we have already
	 * read some of the signature */
	png_set_sig_bytes(png_ptr, sig_read);

	/*
	 * If you have enough memory to read
	 * in the entire image at once, and
	 * you need to specify only
	 * transforms that can be controlled
	 * with one of the PNG_TRANSFORM_*
	 * bits (this presently excludes
	 * dithering, filling, setting
	 * background, and doing gamma
	 * adjustment), then you can read the
	 * entire image (including pixels)
	 * into the info structure with this
	 * call
	 *
	 * PNG_TRANSFORM_STRIP_16 |
	 * PNG_TRANSFORM_PACKING  forces 8 bit
	 * PNG_TRANSFORM_EXPAND forces to
	 *  expand a palette into RGB
	 */
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);

	png_uint_32 read_width, read_height;
	int bit_depth;
	png_get_IHDR(png_ptr, info_ptr, &read_width, &read_height, &bit_depth, &color_type,
				 &interlace_type, NULL, NULL);
	if(width)
		*width = read_width;
	if(height)
		*height = read_height;

	unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	if(data)
	{
		*data = (unsigned char*) malloc(row_bytes * read_height);

		png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);

		png_uint_32 i;
		for (i = 0; i < read_height; i++)
		{
			memcpy(*data+(row_bytes * (i)), row_pointers[i], row_bytes);
		}
	}

	/* Clean up after the read,
	 * and free any memory allocated */
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);


	/* Close the file */
	fclose(fp);

	/* That's it */
	return true;
}
bool whitgl_sys_save_png(const char *name, whitgl_int width, whitgl_int height, unsigned char *data)
{
	FILE *fp = fopen(name, "wb");
	if(!fp) return false;

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) return false;

	png_infop info = png_create_info_struct(png);
	if (!info) return false;

	if (setjmp(png_jmpbuf(png))) return false;

	png_init_io(png, fp);

	png_uint_32 png_width = width;
	png_uint_32 png_height = height;

	// Output is 8bit depth, RGBA format.
	png_set_IHDR(
		png,
		info,
		png_width, png_height,
		8,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);
	png_write_info(png, info);

	// To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
	// Use png_set_filler().
	//png_set_filler(png, 0, PNG_FILLER_AFTER);

	png_byte ** row_pointers = png_malloc (png, png_height * sizeof (png_byte *));
	whitgl_int y;
	for (y = 0; y < height; ++y)
	{
		whitgl_int size = sizeof (uint8_t) * png_width * 4;
		// png_byte *row = png_malloc (png, sizeof (uint8_t) * png_width * 4);
		row_pointers[height-1-y] = &data[size*y];
	}

	png_write_image(png, row_pointers);
	png_write_end(png, NULL);

	fclose(fp);

	return true;
}

void whitgl_sys_add_image_from_data(int id, whitgl_ivec size, unsigned char* data)
{
	if(num_images >= WHITGL_IMAGE_MAX)
	{
		WHITGL_PANIC("ERR Too many images");
		return;
	}

	images[num_images].size = size;
	GL_CHECK( glPixelStorei(GL_UNPACK_ALIGNMENT, 1) );
	GL_CHECK( glPixelStorei(GL_PACK_LSB_FIRST, 1) );
	GL_CHECK( glGenTextures(1, &images[num_images].gluint ) );
	GL_CHECK( glActiveTexture( GL_TEXTURE0 ) );
	GL_CHECK( glBindTexture(GL_TEXTURE_2D, images[num_images].gluint) );
	GL_CHECK( glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
	GL_CHECK( glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );
	GL_CHECK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST) );
	GL_CHECK( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST) );
	GL_CHECK( glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x,
				 size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
				 data) );

	images[num_images].id = id;
	num_images++;
}
void whitgl_sys_capture_frame(const char *name)
{
	capture = whitgl_frame_capture_zero;
	capture.do_next = true;
	strncpy(capture.file, name, sizeof(capture.file));
}

void whitgl_sys_update_image_from_data(int id, whitgl_ivec size, unsigned char* data)
{
	int index = -1;
	int i;
	for(i=0; i<num_images; i++)
	{
		if(images[i].id == id)
		{
			index = i;
			break;
		}
	}
	if(index == -1)
	{
		WHITGL_PANIC("ERR Cannot find image %d", id);
		return;
	}
	if(images[index].size.x != size.x || images[index].size.y != size.y)
	{
		WHITGL_PANIC("ERR Image sizes don't match");
		return;
	}
	GL_CHECK( glActiveTexture( GL_TEXTURE0 ) );
	GL_CHECK( glBindTexture(GL_TEXTURE_2D, images[index].gluint) );
	GL_CHECK( glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x,
				 size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
				 data) );
}

void whitgl_sys_add_image(int id, const char* filename)
{
	GLubyte *textureImage;
	whitgl_ivec size;
	WHITGL_LOG("Adding image: %s", filename);
	bool success = whitgl_sys_load_png(filename, &size.x, &size.y, &textureImage);
	if(!success)
	{
		WHITGL_PANIC("loadPngImage error");
	}
	whitgl_sys_add_image_from_data(id, size, textureImage);
	free(textureImage);
}

double whitgl_sys_get_time()
{
	return glfwGetTime();
}

whitgl_sys_color whitgl_sys_color_blend(whitgl_sys_color a, whitgl_sys_color b, whitgl_float factor)
{
	whitgl_sys_color out;
	whitgl_int fac = 256*factor;
	whitgl_int inv = 256*(1-factor);
	out.r = (a.r*inv + b.r*fac)>>8;
	out.g = (a.g*inv + b.g*fac)>>8;
	out.b = (a.b*inv + b.b*fac)>>8;
	out.a = (a.a*inv + b.a*fac)>>8;
	return out;
}
whitgl_sys_color whitgl_sys_color_multiply(whitgl_sys_color a, whitgl_sys_color b)
{
	whitgl_sys_color out;
	out.r = (((whitgl_int)a.r)*((whitgl_int)b.r))>>8;
	out.g = (((whitgl_int)a.g)*((whitgl_int)b.g))>>8;
	out.b = (((whitgl_int)a.b)*((whitgl_int)b.b))>>8;
	out.a = (((whitgl_int)a.a)*((whitgl_int)b.a))>>8;

	return out;
}

whitgl_bool whitgl_sys_window_focused()
{
	return _windowFocused;
}
