#include "Game.hpp"

#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <algorithm>
#include <string>
#include <cstdlib>

//helper defined later; throws if shader compilation fails:
static GLuint compile_shader(GLenum type, std::string const &source);

Game::Game() {
	{ //create an opengl program to perform sun/sky (well, directional+hemispherical) lighting:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
			"#version 330\n"
			"uniform mat4 object_to_clip;\n"
			"uniform mat4x3 object_to_light;\n"
			"uniform mat3 normal_to_light;\n"
			"layout(location=0) in vec4 Position;\n" //note: layout keyword used to make sure that the location-0 attribute is always bound to something
			"in vec3 Normal;\n"
			"in vec4 Color;\n"
			"out vec3 position;\n"
			"out vec3 normal;\n"
			"out vec4 color;\n"
			"void main() {\n"
			"	gl_Position = object_to_clip * Position;\n"
			"	position = object_to_light * Position;\n"
			"	normal = normal_to_light * Normal;\n"
			"	color = Color;\n"
			"}\n"
		);

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
			"#version 330\n"
			"uniform vec3 sun_direction;\n"
			"uniform vec3 sun_color;\n"
			"uniform vec3 sky_direction;\n"
			"uniform vec3 sky_color;\n"
			"in vec3 position;\n"
			"in vec3 normal;\n"
			"in vec4 color;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	vec3 total_light = vec3(0.0, 0.0, 0.0);\n"
			"	vec3 n = normalize(normal);\n"
			"	{ //sky (hemisphere) light:\n"
			"		vec3 l = sky_direction;\n"
			"		float nl = 0.5 + 0.5 * dot(n,l);\n"
			"		total_light += nl * sky_color;\n"
			"	}\n"
			"	{ //sun (directional) light:\n"
			"		vec3 l = sun_direction;\n"
			"		float nl = max(0.0, dot(n,l));\n"
			"		total_light += nl * sun_color;\n"
			"	}\n"
			"	fragColor = vec4(color.rgb * total_light, color.a);\n"
			"}\n"
		);

		simple_shading.program = glCreateProgram();
		glAttachShader(simple_shading.program, vertex_shader);
		glAttachShader(simple_shading.program, fragment_shader);
		//shaders are reference counted so this makes sure they are freed after program is deleted:
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);

		//link the shader program and throw errors if linking fails:
		glLinkProgram(simple_shading.program);
		GLint link_status = GL_FALSE;
		glGetProgramiv(simple_shading.program, GL_LINK_STATUS, &link_status);
		if (link_status != GL_TRUE) {
			std::cerr << "Failed to link shader program." << std::endl;
			GLint info_log_length = 0;
			glGetProgramiv(simple_shading.program, GL_INFO_LOG_LENGTH, &info_log_length);
			std::vector< GLchar > info_log(info_log_length, 0);
			GLsizei length = 0;
			glGetProgramInfoLog(simple_shading.program, GLsizei(info_log.size()), &length, &info_log[0]);
			std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
			throw std::runtime_error("failed to link program");
		}
	}

	{ //read back uniform and attribute locations from the shader program:
		simple_shading.object_to_clip_mat4 = glGetUniformLocation(simple_shading.program, "object_to_clip");
		simple_shading.object_to_light_mat4x3 = glGetUniformLocation(simple_shading.program, "object_to_light");
		simple_shading.normal_to_light_mat3 = glGetUniformLocation(simple_shading.program, "normal_to_light");

		simple_shading.sun_direction_vec3 = glGetUniformLocation(simple_shading.program, "sun_direction");
		simple_shading.sun_color_vec3 = glGetUniformLocation(simple_shading.program, "sun_color");
		simple_shading.sky_direction_vec3 = glGetUniformLocation(simple_shading.program, "sky_direction");
		simple_shading.sky_color_vec3 = glGetUniformLocation(simple_shading.program, "sky_color");

		simple_shading.Position_vec4 = glGetAttribLocation(simple_shading.program, "Position");
		simple_shading.Normal_vec3 = glGetAttribLocation(simple_shading.program, "Normal");
		simple_shading.Color_vec4 = glGetAttribLocation(simple_shading.program, "Color");
	}

	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::u8vec4 Color;
	};
	static_assert(sizeof(Vertex) == 28, "Vertex should be packed.");

	{ //load mesh data from a binary blob:
		std::cout<<" before loading mesh data "<<std::endl;
		std::ifstream blob(data_path("meshes.blob"), std::ios::binary);
		//The blob will be made up of three chunks:
		// the first chunk will be vertex data (interleaved position/normal/color)
		// the second chunk will be characters
		// the third chunk will be an index, mapping a name (range of characters) to a mesh (range of vertex data)

		//read vertex data:
		std::vector< Vertex > vertices;
		read_chunk(blob, "dat0", &vertices);
		std::cout<<" Read Vertex data "<<std::endl;

		//read character data (for names):
		std::vector< char > names;
		read_chunk(blob, "str0", &names);
		std::cout<<" Read Char data ";

		//read index:
		struct IndexEntry {
			uint32_t name_begin;
			uint32_t name_end;
			uint32_t vertex_begin;
			uint32_t vertex_end;
		}; 

		std::cout<<" Read Index Entry "<<std::endl;

	
	
		static_assert(sizeof(IndexEntry) == 16, "IndexEntry should be packed.");


		std::vector< IndexEntry > index_entries;
		read_chunk(blob, "idx0", &index_entries);

		if (blob.peek() != EOF) {
			std::cerr << "WARNING: trailing data in meshes file." << std::endl;
		}

		//upload vertex data to the graphics card:
		glGenBuffers(1, &meshes_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, meshes_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		std::cout<<"upload vertex data to the graphics card: "<<std::endl;

		//create map to store index entries:
		std::map< std::string, Mesh > index;
		for (IndexEntry const &e : index_entries) {
			if (e.name_begin > e.name_end || e.name_end > names.size()) {
				throw std::runtime_error("invalid name indices in index.");
			}
			if (e.vertex_begin > e.vertex_end || e.vertex_end > vertices.size()) {
				throw std::runtime_error("invalid vertex indices in index.");
			}
			Mesh mesh;
			mesh.first = e.vertex_begin;
			mesh.count = e.vertex_end - e.vertex_begin;
			auto ret = index.insert(std::make_pair(
				std::string(names.begin() + e.name_begin, names.begin() + e.name_end),
				mesh));
			if (!ret.second) {
				throw std::runtime_error("duplicate name in index.");
			}
		}
		std::cout<<"create map to store index entries:"<<std::endl;

		//look up into index map to extract meshes:
		auto lookup = [&index](std::string const &name) -> Mesh {
			auto f = index.find(name);
			if (f == index.end()) {
				throw std::runtime_error("Mesh named '" + name + "' does not appear in index.");
			}
			return f->second;
		};

		gummy_mesh = lookup("Circle");
		riflector_mesh=lookup("Riflector");
		floor_mesh=lookup("Floor");
		goal_mesh=lookup("Goal");
		hole_mesh=lookup("Hole");
		player_mesh=lookup("Player");
		starpoint_mesh=lookup("Starpoint");
		wall_mesh=lookup("Wall");

		std::cout<<"look up into index map to extract meshes:"<<std::endl;

		// hemisphere_mesh=lookup("Hemisphere");
		// cursor_mesh = lookup("Cursor");
		// doll_mesh = lookup("Doll");
		// egg_mesh = lookup("Egg");
		// cube_mesh = lookup("Cube");
	}

	{ //create vertex array object to hold the map from the mesh vertex buffer to shader program attributes:
		glGenVertexArrays(1, &meshes_for_simple_shading_vao);
		glBindVertexArray(meshes_for_simple_shading_vao);
		glBindBuffer(GL_ARRAY_BUFFER, meshes_vbo);
		//note that I'm specifying a 3-vector for a 4-vector attribute here, and this is okay to do:
		glVertexAttribPointer(simple_shading.Position_vec4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Position));

		// It is necessary to specify this
		glEnableVertexAttribArray(simple_shading.Position_vec4);
		if (simple_shading.Normal_vec3 != -1U) {
			glVertexAttribPointer(simple_shading.Normal_vec3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Normal));
			glEnableVertexAttribArray(simple_shading.Normal_vec3);
		}
		if (simple_shading.Color_vec4 != -1U) {
			glVertexAttribPointer(simple_shading.Color_vec4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLbyte *)0 + offsetof(Vertex, Color));
			glEnableVertexAttribArray(simple_shading.Color_vec4);
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	GL_ERRORS();

	//---------------- GAME SETUP-------------
	//set up game board with meshes and rolls:
	//board meshes is a vector of type Mesh
	//matrix.reserve(board_size.x * board_size.y);
	//board_meshes.get_allocator().allocate(board_size.x * board_size.y);
	board_meshes.reserve(board_size.x * board_size.y);
	board_rotations.reserve(board_size.x * board_size.y);

	// What is this? Random number generator
	std::mt19937 mt(0xbead1234);

	int rand_idx;
	// Vector of meshes (Of objects which are going to be place inside the grid)
	std::vector< Mesh const * > meshes{&wall_mesh,&starpoint_mesh,&floor_mesh,&riflector_mesh,&hole_mesh};

	for (uint32_t i = 0; i < board_size.x * board_size.y; ++i) // ??
	{
		if(i==0)
		{
			// Initialize the player at the top left corner of the board
			board_meshes.emplace_back(&floor_mesh);

		}
		else if(i==39)
		{
			board_meshes.emplace_back(&goal_mesh);
		}
		else if(i==42)
		{
			board_meshes.emplace_back(&gummy_mesh);
		}

		else
		{
			rand_idx=mt()%meshes.size();
			//rand_idx=rand()%3;
			std::cout<<"randIdx"<<rand_idx<<std::endl;
			board_meshes.emplace_back(meshes[rand_idx]);
			// board_rotations.emplace_back(glm::quat());

			 if(rand_idx==0) // It is a wall
			 {
			 	wall_indices.push_back(i);  // pushing the index at which the wall is place to the wall indices vector

			 }
			 if(rand_idx==1) // It is a starpoint
			 { 
			 	//starpos=i;
			 	star_indices.push_back(i);
			 	//std::cout<<"starpos"<<starpos<<std::endl;

			 }
			 if(rand_idx==4) // It is a hole
			 { 
			 	//starpos=i;
			 	hole_indices.push_back(i);

			 }
			 if(rand_idx==3) // It is a riflector
			 { 
			 	//starpos=i;
			 	riflector_indices.push_back(i);

			 }
			 
		}
		
	}

}


Game::~Game() {
	glDeleteVertexArrays(1, &meshes_for_simple_shading_vao);
	meshes_for_simple_shading_vao = -1U;

	glDeleteBuffers(1, &meshes_vbo);
	meshes_vbo = -1U;

	glDeleteProgram(simple_shading.program);
	simple_shading.program = -1U;

	GL_ERRORS();
}

bool Game::handle_event(SDL_Event const &evt, glm::uvec2 window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}
	

	// If Reset Button 'R' is pressed
	if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) 
	{
		if (evt.key.keysym.scancode == SDL_SCANCODE_R) {
			controls.reset = (evt.type == SDL_KEYDOWN);
			return true;
		}
	}

	//move player on L/R/U/D press:
	if (evt.type == SDL_KEYDOWN ) {
	//if (evt.type == SDL_KEYDOWN && evt.key.repeat == 0) {
		if (evt.key.keysym.scancode == SDL_SCANCODE_LEFT) 
		{
			controls.slide_left= (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_RIGHT) 
		{
			controls.slide_right= (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_UP) 
		{
			controls.slide_up= (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_DOWN) 
		{
			controls.slide_down= (evt.type == SDL_KEYDOWN);
			return true;
		}
	}
	return false;
}


bool Game::check_collision(int x, int y, int board_width,int board_height,std::vector<int>&wall_indices)
{
	//std::cout<<"check collision"<<std::endl;
	int key;
	// Convert cursor x, y to 1 D using column major
	key=y*board_width+x;
	if(std::find(wall_indices.begin(),wall_indices.end(),key)!=wall_indices.end())
	{
		std::cout<<"collision detected--->"<<std::endl;
		return true;
	}
		
	return false;
}

bool Game::check_objects_hit(int x,int y,int board_width,int board_height,std::vector<int>&star_indices)
{
	//std::cout<<"update starpoints"<<std::endl;
	int key;
	// Convert cursor x, y to 1 D using column major
	key=y*board_width+x;

	//std::cout<<"key"<<key;
	if(std::find(star_indices.begin(),star_indices.end(),key)!=star_indices.end())
	{
		std::cout<<"Starpoint collected-->";
		return true;
		
	}

	return false;
}



void Game::update(float elapsed) {
	//if the roll keys are pressed, rotate everything on the same row or column as the cursor:

	glm::quat dr = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	float amt = elapsed * 1.0f;

	if (controls.roll_left) {
		dr = glm::angleAxis(amt, glm::vec3(0.0f, 1.0f, 0.0f)) * dr;
	}
	if (controls.roll_right) {
		dr = glm::angleAxis(-amt, glm::vec3(0.0f, 1.0f, 0.0f)) * dr;
	}
	if (controls.roll_up) {
		dr = glm::angleAxis(amt, glm::vec3(1.0f, 0.0f, 0.0f)) * dr;
	}
	if (controls.roll_down) {
		dr = glm::angleAxis(-amt, glm::vec3(1.0f, 0.0f, 0.0f)) * dr;
	}

	if (controls.slide_up) 
	{
		//std::cout<<"UP"<<std::endl;
		if (cursor.y + 1 < board_size.y) 
			{
				//Collision Detection
				if((Game::check_collision(cursor.x,cursor.y+1,board_size.x,board_size.y,wall_indices)))
				{
					cursor.y=cursor.y;
				}
				// Starpoint detection
				else if((Game::check_objects_hit(cursor.x,cursor.y+1,board_size.x,board_size.y,star_indices))) 
				{
					cursor.y += 1;
					star_points+=1;

				}
				else if((Game::check_objects_hit(cursor.x,cursor.y-1,board_size.x,board_size.y,hole_indices))) 
				{
					cursor.y -= 1;
					star_points-=1;
					hole_points+=1;
				}
				else if((Game::check_objects_hit(cursor.x,cursor.y+1,board_size.x,board_size.y,riflector_indices))) 
				{
					cursor.y += 1;
					cursor.x+=1;
				
				}
				else
				{
					cursor.y += 1;
				}

			}
		controls.slide_up=false;
	}
	if (controls.slide_down) 
	{
		//std::cout<<"DOWN"<<std::endl;
		if (cursor.y > 0) 
			{
				if((Game::check_collision(cursor.x,cursor.y-1,board_size.x,board_size.y,wall_indices)))
				{
					cursor.y=cursor.y;
				}
				else if((Game::check_objects_hit(cursor.x,cursor.y-1,board_size.x,board_size.y,star_indices))) 
				{
					cursor.y -= 1;
					star_points+=1;
				}
				else if((Game::check_objects_hit(cursor.x,cursor.y-1,board_size.x,board_size.y,hole_indices))) 
				{
					cursor.y -= 1;
					star_points-=1;
					hole_points+=1;
				}
				else if((Game::check_objects_hit(cursor.x,cursor.y-1,board_size.x,board_size.y,riflector_indices))) 
				{
					cursor.y -= 1;
					cursor.x+=1;

				
				}
				else
				{
					cursor.y -= 1;
				}
				
			}
		controls.slide_down=false;
	}
	if (controls.slide_left) 
	{
		//std::cout<<"LEFT"<<std::endl;
		if (cursor.x > 0) 
			{
				if((Game::check_collision(cursor.x-1,cursor.y,board_size.x,board_size.y,wall_indices)))
				{
					cursor.y=cursor.y;
				}
				else if((Game::check_objects_hit(cursor.x-1,cursor.y,board_size.x,board_size.y,star_indices))) 
				{
					cursor.x -= 1;
					star_points+=1;
				}
				else if((Game::check_objects_hit(cursor.x-1,cursor.y,board_size.x,board_size.y,hole_indices))) 
				{
					cursor.x -= 1;
					star_points-=1;
					hole_points+=1;
				}
				else if((Game::check_objects_hit(cursor.x-1,cursor.y,board_size.x,board_size.y,riflector_indices))) 
				{
					cursor.y += 1;
					cursor.x-=1;

				
				}
				else
				{
					cursor.x -= 1;
				}

			}

		controls.slide_left=false;

	}
	if (controls.slide_right) 
	{
		//std::cout<<"RIGHT"<<std::endl;
		if (cursor.x + 1 < board_size.x) 
			{
				if((Game::check_collision(cursor.x+1,cursor.y,board_size.x,board_size.y,wall_indices)))
				{
					cursor.y=cursor.y;
				}
				else if((Game::check_objects_hit(cursor.x+1,cursor.y,board_size.x,board_size.y,star_indices))) 
				{
					cursor.x += 1;
					star_points+=1;
				}
				else if((Game::check_objects_hit(cursor.x+1,cursor.y,board_size.x,board_size.y,hole_indices))) 
				{
					cursor.x += 1;
					star_points-=1;
					hole_points+=1;
				}
				else if((Game::check_objects_hit(cursor.x+1,cursor.y,board_size.x,board_size.y,riflector_indices))) 
				{
					cursor.y -= 1;
					cursor.x+=1;
				
				}
				else
				{
					cursor.x+= 1;
				}


				
			}
		controls.slide_right=false;
	}

	// Function for Reset
	if (controls.reset){
		std::cout<<"Reset function"<<std::endl;
		//Game::reset();
	}


	if (dr != glm::quat()) {
		for (uint32_t x = 0; x < board_size.x; ++x) {
			glm::quat &r = board_rotations[cursor.y * board_size.x + x];
			r = glm::normalize(dr * r);
		}
		for (uint32_t y = 0; y < board_size.y; ++y) {
			if (y != cursor.y) {
				glm::quat &r = board_rotations[y * board_size.x + cursor.x];
				r = glm::normalize(dr * r);
			}
		}
	}
}

void Game::draw(glm::uvec2 drawable_size) {
	//Set up a transformation matrix to fit the board in the window:
	glm::mat4 world_to_clip;
	{
		float aspect = float(drawable_size.x) / float(drawable_size.y);

		//want scale such that board * scale fits in [-aspect,aspect]x[-1.0,1.0] screen box:
		float scale = glm::min(
			2.0f * aspect / float(board_size.x),
			2.0f / float(board_size.y)
		);

		//center of board will be placed at center of screen:
		glm::vec2 center = 0.5f * glm::vec2(board_size);

		//NOTE: glm matrices are specified in column-major order
		world_to_clip = glm::mat4(
			scale / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, scale, 0.0f, 0.0f,
			0.0f, 0.0f,-1.0f, 0.0f,
			-(scale / aspect) * center.x, -scale * center.y, 0.0f, 1.0f
		);
	}

	//set up graphics pipeline to use data from the meshes and the simple shading program:
	glBindVertexArray(meshes_for_simple_shading_vao);
	glUseProgram(simple_shading.program);

	glUniform3fv(simple_shading.sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(simple_shading.sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(simple_shading.sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.3f)));
	glUniform3fv(simple_shading.sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));

	//helper function to draw a given mesh with a given transformation:
	auto draw_mesh = [&](Mesh const &mesh, glm::mat4 const &object_to_world) {
		//set up the matrix uniforms:
		if (simple_shading.object_to_clip_mat4 != -1U) {
			glm::mat4 object_to_clip = world_to_clip * object_to_world;
			glUniformMatrix4fv(simple_shading.object_to_clip_mat4, 1, GL_FALSE, glm::value_ptr(object_to_clip));
		}
		if (simple_shading.object_to_light_mat4x3 != -1U) {
			glUniformMatrix4x3fv(simple_shading.object_to_light_mat4x3, 1, GL_FALSE, glm::value_ptr(object_to_world));
		}
		if (simple_shading.normal_to_light_mat3 != -1U) {
			//NOTE: if there isn't any non-uniform scaling in the object_to_world matrix, then the inverse transpose is the matrix itself, and computing it wastes some CPU time:
			glm::mat3 normal_to_world = glm::inverse(glm::transpose(glm::mat3(object_to_world)));
			glUniformMatrix3fv(simple_shading.normal_to_light_mat3, 1, GL_FALSE, glm::value_ptr(normal_to_world));
		}

		//draw the mesh:
		glDrawArrays(GL_TRIANGLES, mesh.first, mesh.count);
	};

	for (uint32_t y = 0; y < board_size.y; ++y) 
	{
		for (uint32_t x = 0; x < board_size.x; ++x) {
			draw_mesh(floor_mesh,
				glm::mat4(
					1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
					x+0.5f, y+0.5f,-0.5f, 1.0f
				)
			);
			draw_mesh(*board_meshes[y*board_size.x+x],
				glm::mat4(
					1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
					x+0.5f, y+0.5f, 0.0f, 1.0f
				)
				* glm::mat4_cast(board_rotations[y*board_size.x+x])
			);
		}
	}
	draw_mesh(player_mesh,
		glm::mat4(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			cursor.x+0.5f, cursor.y+0.5f, 0.0f, 1.0f
		)
	);

	std::cout<<"points"<<star_points<<std::endl;

   // For points decrement
	if(hole_points!=0)
	{
		for(int i=0;i<hole_points;i++)
		{
				draw_mesh(hole_mesh,
			glm::mat4(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				board_size.x+0.5f,i, 0.0f, 1.0f
			)	
			);

		}
	 }

	// for points increment
	if(star_points>total_points)
	{
		for(int i=0;i<total_points;i++)
		{
				draw_mesh(starpoint_mesh,
			glm::mat4(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				board_size.x+0.5f,i, 0.0f, 1.0f
			)	
			);

		}
	 }

	glUseProgram(0);

	GL_ERRORS();
}



//create and return an OpenGL vertex shader from source:
static GLuint compile_shader(GLenum type, std::string const &source) {
	GLuint shader = glCreateShader(type);
	GLchar const *str = source.c_str();
	GLint length = GLint(source.size());
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, GLsizei(info_log.size()), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}
