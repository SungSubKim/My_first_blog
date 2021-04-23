#include "cgmath.h"			// slee's simple math library
#define STB_IMAGE_IMPLEMENTATION
#include "cgut2.h"			// slee's OpenGL utility
#include "trackball.h"
#include "assimp_loader.h"
#include "circle.h"

//*************************************
// global constants
static const char*	window_name = "cgmodel - assimp for loading {obj|3ds} files";
static const char*	vert_shader_path = "../bin/shaders/model.vert";
static const char*	frag_shader_path = "../bin/shaders/model.frag";
static const char*	mesh_obj = "../bin/mesh/Map2_black.obj";

std::vector<vertex>	unit_circle_vertices;	// host-side vertices
//*************************************
// common structures
struct camera
{
	vec3	eye = vec3(0, 50, 120 );
	vec3	at = vec3( -2, 0, 0 );
	vec3	up = vec3( 0, 1, 0 );
	mat4	view_matrix = mat4::look_at( eye, at, up );
		
	float	fovy = PI/4.0f; // must be in radian
	float	aspect_ratio;
	float	dNear = 1.0f;
	float	dFar = 1000.0f;
	mat4	projection_matrix;
};

auto	circles = std::move(create_circles());
GLuint vertex_buffer = 0;	// ID holder for vertex buffer
GLuint index_buffer = 0;		// ID holder for index buffer
uint NUM_VERTEX = (36 + 1)*(72 + 1);
GLuint	vertex_array = 0;	// ID holder for vertex array object
std::vector<vertex> create_planet_vertices()
{
	std::vector<vertex> v;
	uint lan = 36, lon = 72;
	for (uint i = 0; i < lan + 1; i++) // langitude
	{
		float theta = PI * i / float(lan), c = cos(theta), s = sin(theta);
		for (uint j = 0; j < lon + 1; j++) {
			float p = 2 * PI * j / float(lon), c2 = cos(p), s2 = sin(p);
			v.push_back({ vec3(s * c2,s * s2,c), vec3(s * c2,s * s2,c), vec2(p / 2.0f / PI,theta / PI) });
		}
	}
	return v;
}
void update_planet_buffer(const std::vector<vertex>& vertices)
{

	// clear and create new buffers
	if (vertex_buffer)	glDeleteBuffers(1, &vertex_buffer);	vertex_buffer = 0;
	if (index_buffer)	glDeleteBuffers(1, &index_buffer);	index_buffer = 0;

	// check exceptions
	if (vertices.empty()) { printf("[error] vertices is empty.\n"); return; }

	// create buffers
	std::vector<uint> indices;
	for (uint k = 0; k < NUM_VERTEX - 74; k++)
	{
		if (k % 73 == 72)
			continue;
		indices.push_back(k);
		indices.push_back((k + 73));
		indices.push_back((k + 1));

		indices.push_back(k + 1);
		indices.push_back(k + 73);
		indices.push_back(k + 74);
	}

	// generation of vertex buffer: use vertices as it is
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * vertices.size(), &vertices[0], GL_STATIC_DRAW);

	// geneation of index buffer
	glGenBuffers(1, &index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint) * indices.size(), &indices[0], GL_STATIC_DRAW);

	// generate vertex array object, which is mandatory for OpenGL 3.3 and higher
	if (vertex_array) glDeleteVertexArrays(1, &vertex_array);
	vertex_array = cg_create_vertex_array(vertex_buffer, index_buffer);
	if (!vertex_array) { printf("%s(): failed to create vertex aray\n", __func__); return; }
}
//*************************************
// window objects
GLFWwindow*	window = nullptr;
ivec2		window_size = ivec2(1280, 720); // cg_default_window_size(); // initial window size

//*************************************
// OpenGL objects
GLuint	program	= 0;	// ID holder for GPU program

//*************************************
// global variables
int		frame = 0;		// index of rendering frames
bool	show_texcoord = false;
bool	b_wireframe = false;
//*************************************
// scene objects
mesh2*		pMesh = nullptr;
camera		cam;
trackball	tb;
bool l = false, r = false, u = false, d = false;
float old_t=0;
mat4 model_matrix0;
//*************************************
float min(float a, float b) {
	return a < b ? a : b;
}
void update()
{
	glUseProgram(program);

	// update projection matrix
	cam.aspect_ratio = window_size.x/float(window_size.y);
	cam.projection_matrix = mat4::perspective( cam.fovy, cam.aspect_ratio, cam.dNear, cam.dFar );

	// build the model matrix for oscillating scale
	float t = float(glfwGetTime());
	if (l) {
		circles[0].center.x -= (t - old_t) * 50;
		//printf("%f\n",circles[0].center.y);
	}
	else if (r)
		circles[0].center.x += (t - old_t) * 50;
	else if (u)
		circles[0].center.z -= (t - old_t) * 50;
	else if (d)
		circles[0].center.z += (t - old_t) * 50;
	old_t = t;
	float scale = 1.0f;
	mat4 model_matrix = mat4::scale( scale, scale, scale );

	// update uniform variables in vertex/fragment shaders
	GLint uloc;
	uloc = glGetUniformLocation( program, "view_matrix" );			if(uloc>-1) glUniformMatrix4fv( uloc, 1, GL_TRUE, cam.view_matrix );
	uloc = glGetUniformLocation( program, "projection_matrix" );	if(uloc>-1) glUniformMatrix4fv( uloc, 1, GL_TRUE, cam.projection_matrix );
	uloc = glGetUniformLocation( program, "model_matrix" );			if(uloc>-1) glUniformMatrix4fv( uloc, 1, GL_TRUE, model_matrix );

	glActiveTexture(GL_TEXTURE0);								// select the texture slot to bind
}
float old_t2 = 0;
void render()
{
	// clear screen (with background color) and clear depth buffer
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	
	// notify GL that we use our own program
	glUseProgram( program );

	// bind vertex array object
	glBindVertexArray( pMesh->vertex_array );

	for( size_t k=0, kn=pMesh->geometry_list.size(); k < kn; k++ ) {
		geometry& g = pMesh->geometry_list[k];

		if (g.mat->textures.diffuse) {
			glBindTexture(GL_TEXTURE_2D, g.mat->textures.diffuse->id);
			glUniform1i(glGetUniformLocation(program, "TEX"), 0);	 // GL_TEXTURE0
			glUniform1i(glGetUniformLocation(program, "use_texture"), true);
		} else {
			glUniform4fv(glGetUniformLocation(program, "diffuse"), 1, (const float*)(&g.mat->diffuse));
			glUniform1i(glGetUniformLocation(program, "use_texture"), false);
		}

		// render vertices: trigger shader programs to process vertex data
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, pMesh->index_buffer );
		glDrawElements( GL_TRIANGLES, g.index_count, GL_UNSIGNED_INT, (GLvoid*)(g.index_start*sizeof(GLuint)) );
	}

	// swap front and back buffers, and display to screen
	//glfwSwapBuffers( window );
	// bind vertex array object

	glBindVertexArray(vertex_array);
	float t = float(glfwGetTime());
	glUniform1i(glGetUniformLocation(program, "circle"), true);
	// render two circles: trigger shader program to process vertex data
	for (auto& c : circles)
	{
		// per-circle update
		c.update(t);
		// update per-circle uniforms
		GLint uloc;
		uloc = glGetUniformLocation(program, "model_matrix");		if (uloc > -1) glUniformMatrix4fv(uloc, 1, GL_TRUE, c.model_matrix);

		// per-circle draw calls
		glDrawElements(GL_TRIANGLES, (NUM_VERTEX - 73) * 3 * 2, GL_UNSIGNED_INT, nullptr);
	}
	glUniform1i(glGetUniformLocation(program, "circle"), false);
	// swap front and back buffers, and display to screen
	glfwSwapBuffers(window);
}

void reshape( GLFWwindow* window, int width, int height )
{
	// set current viewport in pixels (win_x, win_y, win_width, win_height)
	// viewport: the window area that are affected by rendering 
	window_size = ivec2(width,height);
	glViewport( 0, 0, width, height );
}

void print_help()
{
	printf( "[help]\n" );
	printf( "- press ESC or 'q' to terminate the program\n" );
	printf( "- press F1 or 'h' to see help\n" );
	printf( "- press Home to reset camera\n" );
	printf( "- press 'd' to toggle between OBJ format and 3DS format\n" );
	printf( "\n" );
}

void keyboard( GLFWwindow* window, int key, int scancode, int action, int mods )
{
	if(action==GLFW_PRESS)
	{
		if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q)	glfwSetWindowShouldClose(window, GL_TRUE);
		else if (key == GLFW_KEY_H || key == GLFW_KEY_F1)	print_help();
		else if (key == GLFW_KEY_HOME)					cam = camera();
		else if (key == GLFW_KEY_T)					show_texcoord = !show_texcoord;
		else if (key == GLFW_KEY_D)
		{
			static bool is_obj = true;
			is_obj = !is_obj;

			glFinish();
			delete_texture_cache();
			delete pMesh;
			pMesh = load_model(mesh_obj);
		}
#ifndef GL_ES_VERSION_2_0
		else if (key == GLFW_KEY_W)
		{
			b_wireframe = !b_wireframe;
			glPolygonMode(GL_FRONT_AND_BACK, b_wireframe ? GL_LINE : GL_FILL);
			printf("> using %s mode\n", b_wireframe ? "wireframe" : "solid");
		}
		else if (key == GLFW_KEY_LEFT) {
			l = true;
			old_t2 = (float)glfwGetTime();
		}
			
		else if (key == GLFW_KEY_RIGHT)
			r = true;
		else if (key == GLFW_KEY_UP)
			u = true;
		else if (key == GLFW_KEY_DOWN)
			d = true;
#endif
	}
	else if (action == GLFW_RELEASE) {
		if (key == GLFW_KEY_LEFT)
			l = false;
		else if (key == GLFW_KEY_RIGHT)
			r = false;
		else if (key == GLFW_KEY_UP)
			u = false;
		else if (key == GLFW_KEY_DOWN)
			d = false;
	}
}

void mouse( GLFWwindow* window, int button, int action, int mods )
{
	if(button==GLFW_MOUSE_BUTTON_LEFT)
	{
		dvec2 pos; glfwGetCursorPos(window,&pos.x,&pos.y);
		vec2 npos = cursor_to_ndc( pos, window_size );
		if(action==GLFW_PRESS)			tb.begin( cam.view_matrix, npos );
		else if(action==GLFW_RELEASE)	tb.end();
	}
}

void motion( GLFWwindow* window, double x, double y )
{
	if(!tb.is_tracking()) return;
	vec2 npos = cursor_to_ndc( dvec2(x,y), window_size );
	cam.view_matrix = tb.update( npos );
}

bool user_init()
{
	// log hotkeys
	print_help();

	// init GL states
	glClearColor( 0/255.0f, 0/255.0f, 0/255.0f, 1.0f );	// set clear color
	glEnable( GL_CULL_FACE );								// turn on backface culling
	glEnable( GL_DEPTH_TEST );								// turn on depth tests
	glEnable( GL_TEXTURE_2D );
	glActiveTexture( GL_TEXTURE0 );

	// load the mesh
	pMesh = load_model(mesh_obj);
	if(pMesh==nullptr){ printf( "Unable to load mesh\n" ); return false; }
	// define the position of four corner vertices
	unit_circle_vertices = std::move(create_planet_vertices());

	// create vertex buffer; called again when index buffering mode is toggled
	update_planet_buffer(unit_circle_vertices);

	return true;
}

void user_finalize()
{
	delete_texture_cache();
	delete pMesh;
}

int main( int argc, char* argv[] )
{
	// create window and initialize OpenGL extensions
	if(!(window = cg_create_window( window_name, window_size.x, window_size.y ))){ glfwTerminate(); return 1; }
	if(!cg_init_extensions( window )){ glfwTerminate(); return 1; }	// version and extensions

	// initializations and validations
	if(!(program=cg_create_program( vert_shader_path, frag_shader_path ))){ glfwTerminate(); return 1; }	// create and compile shaders/program
	if(!user_init()){ printf( "Failed to user_init()\n" ); glfwTerminate(); return 1; }					// user initialization

	// register event callbacks
	glfwSetWindowSizeCallback( window, reshape );	// callback for window resizing events
    glfwSetKeyCallback( window, keyboard );			// callback for keyboard events
	glfwSetMouseButtonCallback( window, mouse );	// callback for mouse click inputs
	glfwSetCursorPosCallback( window, motion );		// callback for mouse movement

	// enters rendering/event loop
	for( frame=0; !glfwWindowShouldClose(window); frame++ )
	{
		glfwPollEvents();	// polling and processing of events
		update();			// per-frame update
		render();			// per-frame render
	}
	
	// normal termination
	user_finalize();
	cg_destroy_window(window);

	return 0;
}

