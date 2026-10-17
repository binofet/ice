#include "arcade_grade.h"
#include "ice/globals.h"
#include <cstdlib>
void ArcadeGrade::release() {
	shader.release();
	if (texture)
		glDeleteTextures(1, &texture);
	texture = 0;
	w = h     = 0;
	attempted = false;
}
void ArcadeGrade::apply(int width, int height) {
	if (width <= 0 || height <= 0)
		return;
	if (!attempted) {
		attempted = true;
		if (std::getenv("ICE_DISABLE_SHADERS")) {
			ice::printf("Arcade GLSL disabled; fixed-function fallback");
			return;
		}
		const char *vertex   = "#version 120\nvarying vec2 uv;void "
		                       "main(){uv=gl_MultiTexCoord0.xy;gl_Position=gl_Vertex;}";
		const char *fragment = R"GLSL(#version 120
uniform sampler2D scene;
uniform vec2 pixel;
varying vec2 uv;
void main(){
 vec3 c=texture2D(scene,uv).rgb;
 vec3 glow=vec3(0.0);
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  vec3 tap=texture2D(scene,uv+vec2(float(x),float(y))*pixel*3.0).rgb;
  glow+=max(tap-vec3(0.72),vec3(0.0));
 }
 c+=glow*0.035;
 float l=dot(c,vec3(0.2126,0.7152,0.0722));
 c=mix(vec3(l),c,1.08);
 c=mix(c,c*vec3(0.88,1.02,1.08),0.25*(1.0-l));
 c*=vec3(1.04,1.015,0.98);
 vec2 d=uv*2.0-1.0;
 c*=1.0-0.12*dot(d,d);
 gl_FragColor=vec4(clamp(c,0.0,1.0),1.0);
})GLSL";
		std::string log;
		if (!shader.compile(vertex, fragment, log)) {
			ice::warningf("Arcade shader fallback: %s", log.c_str());
			return;
		}
		ice::printf("Arcade GLSL 1.20 linked: color grade / highlight glow / vignette");
	}
	if (!shader.valid())
		return;
	GLint previousProgram = 0, active = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
	glActiveTexture(GL_TEXTURE0);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	if (!texture)
		glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	if (w != width || h != height) {
		w = width;
		h = height;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	}
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_ALPHA_TEST);
	glDepthMask(GL_FALSE);
	shader.bind();
	glUniform1i(shader.uniform("scene"), 0);
	glUniform2f(shader.uniform("pixel"), 1.0f / w, 1.0f / h);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(-1, -1);
	glTexCoord2f(1, 0);
	glVertex2f(1, -1);
	glTexCoord2f(1, 1);
	glVertex2f(1, 1);
	glTexCoord2f(0, 1);
	glVertex2f(-1, 1);
	glEnd();
	glUseProgram(previousProgram);
	glPopAttrib();
	glActiveTexture(active);
}
