#pragma once

#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include<list>
#include <vector>

// The 'Game' struct holds all of the game-relevant state,
// and is called by the main loop.

struct Game {
	//Game creates OpenGL resources (i.e. vertex buffer objects) in its
	//constructor and frees them in its destructor.
	Game();
	~Game();

	int star_points=0;
	int total_points=5;
	int hole_points=0;
	bool hole_flag=false;
	bool star_flag=false;
	int goal_key=0;
	std::vector<int> wall_indices;
	std::vector<int> star_indices;
	std::vector<int> riflector_indices;
	std::vector<int> hole_indices;


	//handle_event is called when new mouse or keyboard events are received:
	// (note that this might be many times per frame or never)
	//The function should return 'true' if it handled the event.
	bool handle_event(SDL_Event const &evt, glm::uvec2 window_size);

	//update is called at the start of a new frame, after events are handled:
	void update(float elapsed);

	//draw is called after update:
	void draw(glm::uvec2 drawable_size);

	// Reset the game
	//void reset();

	//check collision
	bool check_collision(int x,int y,int board_width,int board_height,std::vector<int> &wall_indices);

	//update Star points
	bool check_objects_hit(int x,int y,int board_width,int board_height,std::vector<int> &star_indices);

	

	

	//------- opengl resources -------

	//shader program that draws lit objects with vertex colors:
	struct {
		GLuint program = -1U; //program object

		//uniform locations:
		GLuint object_to_clip_mat4 = -1U;
		GLuint object_to_light_mat4x3 = -1U;
		GLuint normal_to_light_mat3 = -1U;
		GLuint sun_direction_vec3 = -1U;
		GLuint sun_color_vec3 = -1U;
		GLuint sky_direction_vec3 = -1U;
		GLuint sky_color_vec3 = -1U;

		//attribute locations:
		GLuint Position_vec4 = -1U;
		GLuint Normal_vec3 = -1U;
		GLuint Color_vec4 = -1U;
	} simple_shading;

	//mesh data, stored in a vertex buffer:
	GLuint meshes_vbo = -1U; //vertex buffer holding mesh data

	//The location of each mesh in the meshes vertex buffer:
	struct Mesh {
		GLint first = 0;
		GLsizei count = 0;
	};



	Mesh gummy_mesh;
	Mesh riflector_mesh;
	Mesh floor_mesh;
	Mesh goal_mesh;
	Mesh hole_mesh;
	Mesh player_mesh;
	Mesh wall_mesh;
	Mesh starpoint_mesh;

	Mesh tile_mesh;
	Mesh cursor_mesh;
	Mesh doll_mesh;
	Mesh egg_mesh;
	Mesh cube_mesh;

	//std::vector< Mesh const * > meshes{&wall_mesh,&starpoint_mesh,&gummy_mesh,&floor_mesh};

	GLuint meshes_for_simple_shading_vao = -1U; //vertex array object that describes how to connect the meshes_vbo to the simple_shading_program

	//------- game state -------

	glm::uvec2 board_size = glm::uvec2(8,8);  // Board size 4*4
	//std::vector<std::vector<Mesh const *> > matrix;
	std::vector< Mesh const * > board_meshes;
	//std::vector< Mesh const * > meshes;
	std::vector< glm::quat > board_rotations;

	

	glm::uvec2 cursor = glm::vec2(0,0);

	struct {
		bool slide_left=false;
		bool slide_right=false;
		bool slide_up=false;
		bool slide_down=false;
		bool reset=false;
	} controls;

};
