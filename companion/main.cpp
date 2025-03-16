#include <vitasdk.h>
#include <vitaGL.h>
#include <imgui_vita.h>
#include <stdio.h>
#include <string>
#include "../loader/config.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern "C" {
void *__wrap_memcpy(void *dest, const void *src, size_t n) {
	return sceClibMemcpy(dest, src, n);
}

void *__wrap_memmove(void *dest, const void *src, size_t n) {
	return sceClibMemmove(dest, src, n);
}

void *__wrap_memset(void *s, int c, size_t n) {
	return sceClibMemset(s, c, n);
}
}

char *graphics_presets[] = {
	"Low",
	"Medium",
	"High"
};

char *antialiasing_settings[] = {
	"Disabled",
	"MSAA 2x",
	"MSAA 4x"
};

GLuint antialiasing_presets[3];
GLuint fog_presets[2];
GLuint graphical_presets[3];
GLuint resolution_presets[2];
GLuint hide_presets[2];

game_options opts;
int far_boost_int = 100;

void loadConfig(void) {
	opts.have_fog = 0;
	opts.framerate = 30;
	opts.msaa = 2;
	opts.preset = 2;
	opts.cam_slowdown = 8;
	opts.xperia_res = 1;
	opts.hide_elements = JOYSTICK_ELEM;
	opts.autoframerate = 0;
	
	char buffer[30];
	int value;

	FILE *config = fopen(CONFIG_FILE_PATH, "r");

	if (config) {
		while (EOF != fscanf(config, "%[^ ] %d\n", buffer, &value)) {
			if (strcmp("fps", buffer) == 0) opts.framerate = value;
			else if (strcmp("draw_distance", buffer) == 0) far_boost_int = value;
			else if (strcmp("fog", buffer) == 0) opts.have_fog = value;
			else if (strcmp("msaa", buffer) == 0) opts.msaa = value;
			else if (strcmp("graphics", buffer) == 0) opts.preset = value;
			else if (strcmp("cam_slowdown", buffer) == 0) opts.cam_slowdown = value;
			else if (strcmp("xperia_res", buffer) == 0) opts.xperia_res = value;
			else if (strcmp("hide_elem", buffer) == 0) opts.hide_elements = value;
		}
		fclose(config);
	}
}

void saveConfig(void) {
	FILE *config = fopen(CONFIG_FILE_PATH, "w+");

	if (config) {
		fprintf(config, "%s %d\n", "fps", opts.framerate);
		fprintf(config, "%s %d\n", "draw_distance", far_boost_int);
		fprintf(config, "%s %d\n", "msaa", opts.msaa);
		fprintf(config, "%s %d\n", "fog", opts.have_fog);
		fprintf(config, "%s %d\n", "graphics", opts.preset);
		fprintf(config, "%s %d\n", "cam_slowdown", opts.cam_slowdown);
		fprintf(config, "%s %d\n", "xperia_res", opts.xperia_res);
		fprintf(config, "%s %d\n", "hide_elem", opts.hide_elements);
		fclose(config);
	}
}

char *options_descs[] = {
	"Render the game at 850x480 Xperia resolution. This makes UI and some 2D effects render correctly at the cost of a lower 3D quality.\nThe default value is: Enabled.",
	"Higher settings will make more entities render, higher draw distance for entities and world and more graphical effects.\nThe default value is: High.",
	"Increasing this value will make the game render farther world geometries. Entities rendering is untouched.\nThe default value is: 100%.",
	"Make the game render fog for distant geometries.\nThe default value is: Disabled.",
	"Technique used to reduce aliasing surrounding 3D models. Greatly improves graphics quality at the cost of some GPU power.\nThe default value is: MSAA 4x.",
	"Framerate at which to tie the game.\nThe default value is: 30.",
	"Right stick camera movement sensitivity slowdown. Higher values will make the camera move less with analog movement.\nThe default value is: 8.",
	"Hide the touch keypad on the screen. (Functionality is still active).\nThe default value is: Disabled.",
	"Hide the touch joystick on the screen. (Functionality is still active).\nThe default value is: Enabled.",
};

enum {
	OPT_RESOLUTION_PRESET,
	OPT_GRAPHICAL_PRESET,
	OPT_DRAW_DISTANCE_QUALITY,
	OPT_FOG,
	OPT_ANTIALIASING,
	OPT_FRAMERATE,
	OPT_CAMERA_SLOWDOWN,
	OPT_KEYPAD,
	OPT_JOYSTICK,
};

char *desc = nullptr;

void SetDescription(int i) {
	if (ImGui::IsItemHovered())
		desc = options_descs[i];
}

void uploadTexture(GLuint tex, uint8_t *buf, int w, int h) {
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
}

int main(int argc, char *argv[]) {
	loadConfig();
	int exit_code = 0xDEAD;

	vglInitExtended(0, 960, 544, 0x1800000, SCE_GXM_MULTISAMPLE_4X);
	ImGui::CreateContext();
	ImGui_ImplVitaGL_Init();
	ImGui_ImplVitaGL_TouchUsage(false);
	ImGui_ImplVitaGL_GamepadUsage(true);
	ImGui::StyleColorsDark();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	
	glGenTextures(3, antialiasing_presets);
	glGenTextures(2, fog_presets);
	glGenTextures(3, graphical_presets);
	glGenTextures(2, resolution_presets);
	glGenTextures(2, hide_presets);
	int w, h;
	uint8_t *buf = stbi_load("app0:fog/on.png", &w, &h, NULL, 4);
	uploadTexture(fog_presets[1], buf, w, h);
	free(buf);
	buf = stbi_load("app0:fog/off.png", &w, &h, NULL, 4);
	uploadTexture(fog_presets[0], buf, w, h);
	free(buf);
	buf = stbi_load("app0:antialiasing/0x.png", &w, &h, NULL, 4);
	uploadTexture(antialiasing_presets[0], buf, w, h);
	free(buf);
	buf = stbi_load("app0:antialiasing/2x.png", &w, &h, NULL, 4);
	uploadTexture(antialiasing_presets[1], buf, w, h);
	free(buf);
	buf = stbi_load("app0:antialiasing/4x.png", &w, &h, NULL, 4);
	uploadTexture(antialiasing_presets[2], buf, w, h);
	free(buf);
	buf = stbi_load("app0:presets/low.png", &w, &h, NULL, 4);
	uploadTexture(graphical_presets[0], buf, w, h);
	free(buf);
	buf = stbi_load("app0:presets/mid.png", &w, &h, NULL, 4);
	uploadTexture(graphical_presets[1], buf, w, h);
	free(buf);
	buf = stbi_load("app0:presets/high.png", &w, &h, NULL, 4);
	uploadTexture(graphical_presets[2], buf, w, h);
	free(buf);
	buf = stbi_load("app0:presets/vita.png", &w, &h, NULL, 4);
	uploadTexture(resolution_presets[0], buf, w, h);
	free(buf);
	buf = stbi_load("app0:presets/xperia.png", &w, &h, NULL, 4);
	uploadTexture(resolution_presets[1], buf, w, h);
	free(buf);
	buf = stbi_load("app0:presets/no_joy.png", &w, &h, NULL, 4);
	uploadTexture(hide_presets[JOYSTICK_ELEM - 1], buf, w, h);
	free(buf);
	buf = stbi_load("app0:presets/no_keypad.png", &w, &h, NULL, 4);
	uploadTexture(hide_presets[KEYPAD_ELEM - 1], buf, w, h);
	free(buf);
	
	ImGui::GetIO().MouseDrawCursor = false;
	
	while (exit_code == 0xDEAD) {
		bool show_fog = false;
		bool show_anti = false;
		bool show_graphics = false;
		bool show_reso = false;
		bool show_joy = false;
		bool show_key = false;
		desc = nullptr;
		ImGui_ImplVitaGL_NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiSetCond_Always);
		ImGui::SetNextWindowSize(ImVec2(960, 544), ImGuiSetCond_Always);
		ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

		static bool bool_reso = opts.xperia_res;
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
		ImGui::Text("Xperia Resolution:"); ImGui::SameLine();
		ImGui::Checkbox("##checkreso", &bool_reso);
		ImGui::PopStyleVar();
		if (ImGui::IsItemHovered()) {
			SetDescription(OPT_RESOLUTION_PRESET);
			show_reso = true;
		}
		opts.xperia_res = (int)bool_reso;
		
		static bool bool_fog = opts.have_fog;
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
		ImGui::SameLine(); ImGui::Text("Fog Rendering:"); ImGui::SameLine();
		ImGui::Checkbox("##check1", &bool_fog);
		ImGui::PopStyleVar();
		if (ImGui::IsItemHovered()) {
			SetDescription(OPT_FOG);
			show_fog = true;
		}
		opts.have_fog = (int)bool_fog;
		
		static bool bool_joy = (opts.hide_elements & JOYSTICK_ELEM) == JOYSTICK_ELEM;
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
		ImGui::SameLine(); ImGui::Text("Hide Joystick:"); ImGui::SameLine();
		ImGui::Checkbox("##check2", &bool_joy);
		ImGui::PopStyleVar();
		if (ImGui::IsItemHovered()) {
			SetDescription(OPT_JOYSTICK);
			show_joy = true;
		}
		if (bool_joy)
			opts.hide_elements |= JOYSTICK_ELEM;
		else
			opts.hide_elements &= ~JOYSTICK_ELEM;
		
		static bool bool_key = (opts.hide_elements & KEYPAD_ELEM) == KEYPAD_ELEM;
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
		ImGui::SameLine(); ImGui::Text("Hide Keypad:"); ImGui::SameLine();
		ImGui::Checkbox("##check3", &bool_key);
		ImGui::PopStyleVar();
		if (ImGui::IsItemHovered()) {
			SetDescription(OPT_KEYPAD);
			show_key = true;
		}
		if (bool_key)
			opts.hide_elements |= KEYPAD_ELEM;
		else
			opts.hide_elements &= ~KEYPAD_ELEM;
		
		ImGui::Text("Graphical Preset:"); ImGui::SameLine();
		if (ImGui::BeginCombo("##combo1", graphics_presets[opts.preset])) {
			for (int n = 0; n < (int)(sizeof(graphics_presets)/sizeof(*graphics_presets)); n++) {
				bool is_selected = opts.preset == n;
				if (ImGui::Selectable(graphics_presets[n], is_selected))
					opts.preset = n;
				if (ImGui::IsItemHovered()) {
					show_graphics = true;
					opts.preset = n;
					desc = options_descs[OPT_GRAPHICAL_PRESET];
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered()) {
			show_graphics = true;
			desc = options_descs[OPT_GRAPHICAL_PRESET];
		}
		
		ImGui::Text("Draw Distance:"); ImGui::SameLine();
		ImGui::SliderInt("##drawdist", &far_boost_int, 100, 300);
		if (ImGui::IsItemHovered()) {
			desc = options_descs[OPT_DRAW_DISTANCE_QUALITY];
		}
		
		ImGui::Text("Anti-Aliasing:"); ImGui::SameLine();
		if (ImGui::BeginCombo("##combo2", antialiasing_settings[opts.msaa])) {
			for (int n = 0; n < (int)(sizeof(antialiasing_settings)/sizeof(*antialiasing_settings)); n++) {
				bool is_selected = opts.msaa == n;
				if (ImGui::Selectable(antialiasing_settings[n], is_selected))
					opts.msaa = n;
				if (ImGui::IsItemHovered()) {
					show_anti = true;
					opts.msaa = n;
					desc = options_descs[OPT_ANTIALIASING];
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered()) {
			show_anti = true;
			desc = options_descs[OPT_ANTIALIASING];
		}
		
		ImGui::Text("Framerate:"); ImGui::SameLine();
		ImGui::SliderInt("##framerate", &opts.framerate, 15, 60);
		if (ImGui::IsItemHovered()) {
			desc = options_descs[OPT_FRAMERATE];
		}
		
		ImGui::Text("Camera Slowdown:"); ImGui::SameLine();
		ImGui::SliderInt("##slowdown", &opts.cam_slowdown, 1, 20);
		if (ImGui::IsItemHovered()) {
			desc = options_descs[OPT_CAMERA_SLOWDOWN];
		}
		
		ImGui::Separator();
		if (ImGui::Button("Save and Exit"))
			exit_code = 0;
		ImGui::SameLine();
		if (ImGui::Button("Save and Launch the game"))
			exit_code = 1;
		ImGui::SameLine();
		if (ImGui::Button("Discard and Exit"))
			exit_code = 2;
		ImGui::SameLine();
		if (ImGui::Button("Discard and Launch the game"))
			exit_code = 3;
		ImGui::Separator();

		if (desc) {
			ImGui::Spacing();
			ImGui::Spacing();
			ImGui::TextWrapped(desc);
		}
		if (show_reso) {
			float w = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX((w - 506.47f) / 2.0f);
			ImGui::Image((void*)resolution_presets[opts.xperia_res ? 1 : 0], ImVec2(506.47f, 287.0f));
		}
		if (show_fog) {
			float w = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX((w - 506.47f) / 2.0f);
			ImGui::Image((void*)fog_presets[opts.have_fog ? 1 : 0], ImVec2(506.47f, 287.0f));
		}
		if (show_key) {
			float w = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX((w - 506.47f) / 2.0f);
			ImGui::Image((void*)(bool_key ? hide_presets[KEYPAD_ELEM - 1] : resolution_presets[1]), ImVec2(506.47f, 287.0f));
		}
		if (show_joy) {
			float w = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX((w - 506.47f) / 2.0f);
			ImGui::Image((void*)(bool_joy ? hide_presets[JOYSTICK_ELEM - 1] : resolution_presets[1]), ImVec2(506.47f, 287.0f));
		}
		if (show_anti) {
			float w = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX((w - 406.94f) / 2.0f);
			ImGui::Image((void*)antialiasing_presets[opts.msaa], ImVec2(406.94f, 287.0f));
		}
		if (show_graphics) {
			float w = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX((w - 506.47f) / 2.0f);
			ImGui::Image((void*)graphical_presets[opts.preset], ImVec2(506.47f, 287.0f));
		}

		ImGui::End();
		glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
		ImGui::Render();
		ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
		vglSwapBuffers(GL_FALSE);
	}

	if (exit_code < 2) // Save
		saveConfig();

	if (exit_code % 2 == 1) // Launch
		sceAppMgrLoadExec("app0:/eboot.bin", NULL, NULL);

	return 0;
}
