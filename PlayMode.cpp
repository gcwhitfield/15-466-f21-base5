#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <random>

GLuint phonebank_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > phonebank_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("ring.pnct"));
	phonebank_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > phonebank_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("ring.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = phonebank_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = phonebank_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

WalkMesh const *walkmesh = nullptr;
Load< WalkMeshes > phonebank_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	WalkMeshes *ret = new WalkMeshes(data_path("ring.w"));
	walkmesh = &ret->lookup("WalkMesh");
	return ret;
});

Load< Sound::Sample > phone0sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sounds/1.wav"));
});

Load< Sound::Sample > phone1sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sounds/2.wav"));
});

Load< Sound::Sample > phone2sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sounds/3.wav"));
});

Load< Sound::Sample > phone3sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sounds/4.wav"));
});

Load< Sound::Sample > phone4sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sounds/5.wav"));
});

PlayMode::PlayMode() : scene(*phonebank_scene) {
	//create a player transform:
	scene.transforms.emplace_back();
	player.transform = &scene.transforms.back();

	for (auto &transform : scene.transforms)
	{
		if (transform.name == "Phone0")
		{
			phone0 = &transform;
		}
	}
	
	//create a player camera attached to a child of the player transform:
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	player.camera = &scene.cameras.back();
	player.camera->fovy = glm::radians(60.0f);
	player.camera->near = 0.01f;
	player.camera->transform->parent = player.transform;

	//player's eyes are 1.8 units above the ground:
	player.camera->transform->position = glm::vec3(0.0f, 0.0f, 1.8f);

	//rotate camera facing direction (-z) to player facing direction (+y):
	player.camera->transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	//start player walking at nearest walk point:
	player.at = walkmesh->nearest_walk_point(player.transform->position);
	player.transform->position = walkmesh->to_world_point(player.at);

	// play the phone message
	auto play_sound = [](Load<Sound::Sample> smpl, glm::vec3 pos)
	{
		std::vector< float > data(smpl->data.size());
		// PARANOIA - initialize the data vector to all zeros so that 
		// garbade is never sent to the sound card
		for (float &f : data)
		{
			f = 0.0f;
		}

		for (size_t j = 0; j < smpl->data.size(); j++)
		{
			data[j] = smpl->data[j];
		}
		
		Sound::Sample *newSample = new Sound::Sample(data);
		Sound::play_3D(*newSample, 0.5f, pos, 1.f);
	};

	{ // play phone message based on how many times the window has been
		std::fstream myfile (data_path("gameData.txt"));
		std::string line;
		//std::ifstream file(data_path("gameData.txt"), std::ios::binary);

		// code for reading from files inspired by https://www.w3schools.com/cpp/cpp_files.asp
		getline (myfile, line);
		// Output the text from the file
		if (line == "0")
		{
			time_until_kill = 62.0f;
			play_sound(phone0sample, phone0->position);
			myfile.clear();
			myfile << "1";
		}
		else if (line == "01")
		{
			time_until_kill = 46.0f;
			play_sound(phone1sample, phone0->position);
			myfile.clear();
			myfile << "2";
		}	
		else if (line == "012")
		{
			time_until_kill = 30.0f;
			play_sound(phone2sample, phone0->position);
			myfile.clear();
			myfile << "3";
		}
		else if (line == "0123")
		{
			time_until_kill = 7.0f;
			play_sound(phone3sample, phone0->position);
			myfile.clear();
			myfile << "4";
		}
		else
		{
			time_until_kill = 2.0f;
			play_sound(phone4sample, phone0->position);
			myfile.clear();
		}
	}

	assert(phone0sample->data.size() > 0);
	
	/*
	//font initialization
	{
		{ // vertex buffer:
			glGenBuffers(1, &font_vertex_buffer);
			// for now, buffer will be un-filled.

			GL_ERRORS(); // PARANOIA: print out any OpenGL errors that may have happened
		}

		{ // vertex array mapping buffer for font_program:
			// ask OpenGL to fill font_vertex_attributes with the name of an unused vertex array object:
			glGenVertexArrays(1, &font_vertex_attributes);

			// set font_vertex_attributes as the current vertex array object:
			glBindVertexArray(font_vertex_attributes);

			// set font_vertex_buffer as the source of glVertexAttribPointer() commands:
			glBindBuffer(GL_ARRAY_BUFFER, font_vertex_buffer);

			// set up the vertex array object to describe arrays of MemoryGameMode::Vertex:
			glVertexAttribPointer(
				font_program.Position_vec4, // attribute
				3, // size
				GL_FLOAT, // type
				GL_FALSE, // normalized
				sizeof(Vertex), // stride
				(GLbyte *)0 + 0 // offset
			);
			glEnableVertexAttribArray(font_program.Position_vec4);
			// [Note that it is okay to bind a vec3 input to a vec4 attribute -- the w component will be filled with 1.0 automatically]

			glVertexAttribPointer(
				font_program.Color_vec4, // attribute
				4, // size
				GL_UNSIGNED_BYTE, // type
				GL_TRUE, // normalized
				sizeof(Vertex), // stride
				(GLbyte *)0 + 4*3 // offset
			);
			glEnableVertexAttribArray(font_program.Color_vec4);

			glVertexAttribPointer(
				font_program.TexCoord_vec2, // attribute
				2, // size
				GL_FLOAT, // type
				GL_FALSE, // normalized
				sizeof(Vertex), // stride
				(GLbyte *)0 + 4*3 + 4*1 // offset
			);
			glEnableVertexAttribArray(font_program.TexCoord_vec2);

			// done referring to font_vertex_buffer, so unbind it:
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			// done setting up vertex array object, so unbind it:
			glBindVertexArray(0);

			GL_ERRORS(); // PARANOIA: print out any OpenGL errors that may have happened
		}

		// quicksilverFont.initialize(quicksilverFontFile);
		// quicksilverText = DrawText(
		// 	quicksilverFontFile,
		// 	font_program.program,
		// 	font_vertex_attributes,
		// 	font_vertex_buffer
		// );

		{ // make a texture from FreeType

			// 1) Load font with Freetype
			// copied from https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
			FT_Library ft;
			if (FT_Init_FreeType(&ft))
			{
				std::cout << "ERROR::FREETYPE:: Could not init FreeType Library " << std::endl;
				exit(0);
			}

			const char* fontFile = &"fonts/quicksilver_3/Quicksilver.ttf"[0];
			FT_Face face;
			if (FT_New_Face(ft, fontFile, 0, &face))
			{
				std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
				exit(0);
			}

			// disable alignment since what we read from the face (font) is grey-scale. 
			// this line was copied from https://github.com/ChunanGang/TextBasedGame/blob/main/TextRenderer.cpp
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 

			FT_Set_Pixel_Sizes(face, 0, 48);

			const char* text = &"Hello!"[0];

			// Create hb-ft font
			hb_font_t *hb_font;
			hb_font = hb_ft_font_create(face, NULL);
			
			// Create hb_buffer and populate
			hb_buffer_t *hb_buffer;
			hb_buffer = hb_buffer_create();
			hb_buffer_add_utf8(hb_buffer, text, -1, 0, -1 );
			hb_buffer_guess_segment_properties(hb_buffer);
			
			// shape it!
			hb_shape(hb_font, hb_buffer, NULL, 0);

			// get glyph information and positions out of the buffer
			unsigned int len = hb_buffer_get_length(hb_buffer);
			hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buffer, NULL);
			hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(hb_buffer, NULL);

			// print out the contents as is
			for (size_t i = 0; i < len; i++)
			{
				hb_codepoint_t gid = info[i].codepoint;
				unsigned int cluster = info[i].cluster;
				double x_advance = pos[i].x_advance / 64.;
				double y_advance = pos[i].y_advance / 64.;
				double x_offset = pos[i].x_offset / 64.;
				double y_offset = pos[i].y_offset / 64.;

				char glyphname[32];
				hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof(glyphname));

				printf("glyph='%s' cluster=%d advance=(%g,%g) offset=(%g,%g)\n",
				glyphname, cluster, x_advance, y_advance, x_offset, y_offset);
			}

			// 2) load character with FreeType
			// Font::Character c;
			if (FT_Load_Char(face, 'X', FT_LOAD_RENDER))
			{
				std::cout << "ERROR::FREETYPE: Failed to load Glyph" << std::endl;
				exit(0);
			}

			// 3) Create a texture from glyph (should be 'X')
			// glGenTextures(1, &white_tex);
			// glBindTexture(GL_TEXTURE_2D, white_tex);
			// glm::uvec2 size = glm::uvec2(face->glyph->bitmap.rows,face->glyph->bitmap.width);
			// std::vector< glm::u8vec4 > data(size.x*size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
			// for (size_t i = 0; i < size.y; i++)
			// {
			// 	for (size_t j = 0; j < size.x; j++)
			// 	{
			// 		size_t index = i * size.y + j;
			// 		uint8_t val = face->glyph->bitmap.buffer[j * std::abs(face->glyph->bitmap.pitch) + i]; // copied from professor mccan's example code for printing bitmap buffer
			// 		(void) val;
				
			// 		data[index].x = val;
			// 		data[index].y = val;
			// 		data[index].z = val;
			// 		data[index].w = val;
			// 	}
			// }
			// glTexImage2D(
			// 	GL_TEXTURE_2D,
			// 	0, 
			// 	GL_RGBA,
			// 	size.x,
			// 	size.y,
			// 	0, 
			// 	GL_RGBA,
			// 	GL_UNSIGNED_BYTE,
			// 	data.data()
			// );

			glGenTextures(1, &white_tex);
			glBindTexture(GL_TEXTURE_2D, white_tex);
			// upload a 1x1 image of solid white to the texture:
			glm::uvec2 size = glm::uvec2(face->glyph->bitmap.rows, face->glyph->bitmap.width);
			glm::u8vec4 start_color(0x00, 0x00, 0x00, 0x00);
			glm::u8vec4 end_color(0xff, 0xff, 0xff, 0xff);
			std::vector< glm::u8vec4 > data(size.x*size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
			for (int i = 0; i < size.y; i++)
			{
				for (int j = 0; j < size.x; j++)
				{
					int index = i * size.y + j;
					float t =  sqrt(pow((i / (float)size.y), 2) + pow((j / (float)size.x), 2)) / sqrt(2);
					data[index].x = start_color.x * t + end_color.x * (t - (float)1); 
					data[index].y = start_color.y * t + end_color.y * (t - (float)1); 
					data[index].z = start_color.z * t + end_color.z * (t - (float)1); 
					data[index].w = start_color.w * t + end_color.w * (t - (float)1); 
					// uint8_t val = face->glyph->bitmap.buffer[j * std::abs(face->glyph->bitmap.pitch) + i]; // copied from professor mccan's example code for printing bitmap buffer
					// data[index].x =val; 
					// data[index].y =val; 
					// data[index].z =val; 
					// data[index].w =val; 
				}
			}
			
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

			// set filtering and wrapping parameters:
			// (it's a bit silly to mipmap a 1x1 texture, but I'm doing it because you may want to use this code to load different sizes of texture)
			// parameters copied form https://github.com/ChunanGang/TextBasedGame/blob/main/TextRenderer.cpp
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			// since texture uses a mipmap and we haven't uploaded one, instruct opengl to make one for us:
			glGenerateMipmap(GL_TEXTURE_2D);

			// Okay, texture uploaded, can unbind it:
			glBindTexture(GL_TEXTURE_2D, 0);

			GL_ERRORS(); // PARANOIA: print out any OpenGL errors that may have happened
		}
	}
	*/
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			glm::vec3 up = walkmesh->to_world_smooth_normal(player.at);
			player.transform->rotation = glm::angleAxis(-motion.x * player.camera->fovy, up) * player.transform->rotation;

			float pitch = glm::pitch(player.camera->transform->rotation);
			pitch += motion.y * player.camera->fovy;
			//camera looks down -z (basically at the player's feet) when pitch is at zero.
			pitch = std::min(pitch, 0.95f * 3.1415926f);
			pitch = std::max(pitch, 0.05f * 3.1415926f);
			player.camera->transform->rotation = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	//player walking:
	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 3.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		//get move in world coordinate system:
		glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(move.x, move.y, 0.0f, 0.0f);

		//using a for() instead of a while() here so that if walkpoint gets stuck in
		// some awkward case, code will not infinite loop:
		for (uint32_t iter = 0; iter < 10; ++iter) {
			if (remain == glm::vec3(0.0f)) break;
			WalkPoint end;
			float time;
			walkmesh->walk_in_triangle(player.at, remain, &end, &time);
			player.at = end;
			if (time == 1.0f) {
				//finished within triangle:
				remain = glm::vec3(0.0f);
				break;
			}
			//some step remains:
			remain *= (1.0f - time);
			//try to step over edge:
			glm::quat rotation;
			if (walkmesh->cross_edge(player.at, &end, &rotation)) {
				//stepped to a new triangle:
				player.at = end;
				//std::cout << "player at " << player.at.x << ", " << player.at.y << ", " << player.at.z << std::endl;
				//rotate step to follow surface:
				remain = rotation * remain;
			} else {
				//ran into a wall, bounce / slide along it:
				glm::vec3 const &a = walkmesh->vertices[player.at.indices.x];
				glm::vec3 const &b = walkmesh->vertices[player.at.indices.y];
				glm::vec3 const &c = walkmesh->vertices[player.at.indices.z];
				glm::vec3 along = glm::normalize(b-a);
				glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
				glm::vec3 in = glm::cross(normal, along);

				//check how much 'remain' is pointing out of the triangle:
				float d = glm::dot(remain, in);
				if (d < 0.0f) {
					//bounce off of the wall:
					remain += (-1.25f * d) * in;
				} else {
					//if it's just pointing along the edge, bend slightly away from wall:
					remain += 0.01f * d * in;
				}
			}
		}

		if (remain != glm::vec3(0.0f)) {
			std::cout << "NOTE: code used full iteration budget for walking." << std::endl;
		}

		//  std::cout << "Before update position " << player.transform->position.x << ", " << player.transform->position.y << ", " << player.transform->position.z << std::endl;
		//update player's position to respect walking:
		player.transform->position = walkmesh->to_world_point(player.at);
		// std::cout << "After update position " << player.transform->position.x << ", " << player.transform->position.y << ", " << player.transform->position.z << std::endl;


		{ //update player's rotation to respect local (smooth) up-vector:
			
			glm::quat adjust = glm::rotation(
				player.transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f), //current up vector
				walkmesh->to_world_smooth_normal(player.at) //smoothed up vector at walk location
			);
			player.transform->rotation = glm::normalize(adjust * player.transform->rotation);
		}

		/*
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position += move.x * right + move.y * forward;
		*/
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

	// make the big phone go up and down
	float speed = 0.2f;
	float amp = 2.0f;
	t += elapsed;
	phone0->position = glm::vec3(0, 0, amp * cos(M_2_PI * 2 * speed * t));

	// quit the application after time_to_kill seconds
	if (t > time_until_kill && time_until_kill > 0)
	{
		exit(0);
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	
	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*player.camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));


		// walkmesh drawing code copied from professor mccann discord
		// glDisable(GL_DEPTH_TEST);
		// {
		// 	DrawLines lines(player.camera->make_projection() * glm::mat4(player.camera->transform->make_world_to_local()));
		// 	for (auto const &tri : walkmesh->triangles) {
		// 		lines.draw(walkmesh->vertices[tri.x], walkmesh->vertices[tri.y], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
		// 		lines.draw(walkmesh->vertices[tri.y], walkmesh->vertices[tri.z], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
		// 		lines.draw(walkmesh->vertices[tri.z], walkmesh->vertices[tri.x], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
		// 	}
		// }
		// glEnable(GL_DEPTH_TEST);

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}
