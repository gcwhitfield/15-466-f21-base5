#include "Mode.hpp"

#include "Scene.hpp"
#include "WalkMesh.hpp"
#include "Sound.hpp"
#include "LitColorTextureProgram.hpp"
#include "ColorTextureProgram.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <map>
#include <iostream>
#include <fstream>
#include <string>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode() {};

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//player info:
	struct Player {
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		//camera is at player's head and will be pitched by mouse up/down motion:
		Scene::Camera *camera = nullptr;
	} player;

	Scene::Transform *phone0 = NULL;
	float time_until_kill = 0; // set in constructor
	float t = 0;

	ColorTextureProgram font_program;
	GLuint font_vertex_buffer = -1;
	GLuint font_vertex_attributes = -1;
	GLuint white_tex = -1;

// The vertex class was copied from the NEST framework
// draw functions will work on vectors of vertices, defined as follows:
struct Vertex {
    Vertex(glm::vec3 const &Position_, glm::u8vec4 const &Color_, glm::vec2 const &TexCoord_) :
        Position(Position_), Color(Color_), TexCoord(TexCoord_) { }
    glm::vec3 Position;
    glm::u8vec4 Color;
    glm::vec2 TexCoord;
};

// HEX_TO_U8VEC4 was copied from the NEST framework
// some nice colors from the course web page:
#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
const glm::u8vec4 bg_color = HEX_TO_U8VEC4(0x454500ff);
const glm::u8vec4 fg_color = HEX_TO_U8VEC4(0x4361eeff);

// inline helper functions for drawing shapes. The triangles are being counter clockwise.
// draw_rectangle copied from NEST framework
inline void draw_rectangle (std::vector<Vertex> &verts, glm::vec2 const &center, glm::vec2 const &radius, glm::u8vec4 const &color) {
    verts.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.0f, 0.0f));
    verts.emplace_back(glm::vec3(center.x+radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.0f, 1.0f));
    verts.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(1.0f, 1.0f));

    verts.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.0f, 0.0f));
    verts.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(1.0f, 1.0f));
    verts.emplace_back(glm::vec3(center.x-radius.x, center.y+radius.y, 0.0f), color, glm::vec2(1.0f, 0.0f));
};

inline void draw_quadrilateral (std::vector<Vertex> &verts, glm::vec2 const &top_left, glm::vec2 const &top_right, glm::vec2 const &bot_left, glm::vec2 const &bot_right, glm::u8vec4 const &color) {
    // the body of this function was copied largely from Professor McCann's start code for 'draw_rectangle's
    verts.emplace_back(glm::vec3(top_left.x, top_left.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
    verts.emplace_back(glm::vec3(top_right.x, top_right.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
    verts.emplace_back(glm::vec3(bot_right.x, bot_right.y, 0.0f), color, glm::vec2(0.5f, 0.5f));

    verts.emplace_back(glm::vec3(top_left.x, top_left.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
    verts.emplace_back(glm::vec3(bot_right.x, bot_right.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
    verts.emplace_back(glm::vec3(bot_left.x, bot_left.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
};
};
