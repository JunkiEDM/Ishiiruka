// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "ProgramShaderCache.h"
#include "MathUtil.h"

#include <assert.h>
namespace OGL
{
	GLuint ProgramShaderCache::CurrentFShader = 0, ProgramShaderCache::CurrentVShader = 0, ProgramShaderCache::CurrentProgram = 0;
	ProgramShaderCache::PCache ProgramShaderCache::pshaders;
	GLuint ProgramShaderCache::s_ps_ubo;
	GLuint ProgramShaderCache::s_vs_ubo;
	float* ProgramShaderCache::s_ps_mapped_data;
	float* ProgramShaderCache::s_vs_mapped_data;

	std::pair<u64, u64> ProgramShaderCache::CurrentShaderProgram;
	const char *UniformNames[NUM_UNIFORMS] = {
		// SAMPLERS
			"samp0","samp1","samp2","samp3","samp4","samp5","samp6","samp7",
		// PIXEL SHADER UNIFORMS
			I_COLORS,
			I_KCOLORS,
			I_ALPHA,
			I_TEXDIMS,
			I_ZBIAS ,
			I_INDTEXSCALE ,
			I_INDTEXMTX,
			I_FOG,
			I_PLIGHTS,
			I_PMATERIALS,
		// VERTEX SHADER UNIFORMS
			I_POSNORMALMATRIX,
			I_PROJECTION ,
			I_MATERIALS,
			I_LIGHTS,
			I_TEXMATRICES,
			I_TRANSFORMMATRICES ,
			I_NORMALMATRICES ,
			I_POSTTRANSFORMMATRICES,
			I_DEPTHPARAMS,
		};

	void ProgramShaderCache::SetBothShaders(GLuint PS, GLuint VS)
	{
		PROGRAMUID uid;
		CurrentFShader = PS;
		CurrentVShader = VS;


		GetProgramShaderId(&uid, CurrentVShader, CurrentFShader);

		if(uid.uid.id == 0)
		{
			CurrentProgram = 0;
			glUseProgram(0);
			return;
		}

		// Fragment shaders can survive without Vertex Shaders
		// We have a valid fragment shader, let's create our program
		std::pair<u64, u64> ShaderPair = std::make_pair(uid.uid.psid, uid.uid.vsid);
		PCache::iterator iter = pshaders.find(ShaderPair);
		if (iter != pshaders.end())
		{
			PCacheEntry &entry = iter->second;
			glUseProgram(entry.program.glprogid);
			CurrentShaderProgram = ShaderPair;
			CurrentProgram = entry.program.glprogid;
			return;
		}
		PCacheEntry entry;
		entry.program.vsid = CurrentVShader;
		entry.program.psid = CurrentFShader;
		entry.program.glprogid = glCreateProgram();

		// Right, the program is created now
		// Let's attach everything
		if(entry.program.vsid != 0) // attaching zero vertex shader makes it freak out
			glAttachShader(entry.program.glprogid, entry.program.vsid);
			
		glAttachShader(entry.program.glprogid, entry.program.psid);
		
		glLinkProgram(entry.program.glprogid);
		
		glUseProgram(entry.program.glprogid);

		// Dunno why this is needed when I have the binding
		// points statically set in the shader source
		// We should only need these two functions when we don't support binding but do support UBO
		// Driver Bug? Nvidia GTX 570, 290.xx Driver, Linux x64
		//if(!g_ActiveConfig.backend_info.bSupportsGLSLBinding)
		{
			glUniformBlockBinding( entry.program.glprogid, 0, 1 );
			glUniformBlockBinding( entry.program.glprogid, 1, 2 );
		}
				
		// We cache our uniform locations for now
		// Once we move up to a newer version of GLSL, ~1.30
		// We can remove this
		
		//For some reason this fails on my hardware     
		//glGetUniformIndices(entry.program.glprogid, NUM_UNIFORMS, UniformNames, entry.program.UniformLocations);
		//Got to do it this crappy way.
		if(!g_ActiveConfig.backend_info.bSupportsGLSLUBO)
			for(int a = 0; a < NUM_UNIFORMS; ++a)
				entry.program.UniformLocations[a] = glGetUniformLocation(entry.program.glprogid, UniformNames[a]);

		// Need to get some attribute locations
		if(uid.uid.vsid != 0) // We have no vertex Shader
		{
			entry.program.attrLoc[0] = glGetAttribLocation(entry.program.glprogid, "rawnorm1");
			entry.program.attrLoc[1] = glGetAttribLocation(entry.program.glprogid, "rawnorm2");
			entry.program.attrLoc[2] = glGetAttribLocation(entry.program.glprogid, "fposmtx");
			if(entry.program.attrLoc[0] > 0)
				glEnableVertexAttribArray(entry.program.attrLoc[0]);
			if(entry.program.attrLoc[1] > 0)
				glEnableVertexAttribArray(entry.program.attrLoc[1]);
			if(entry.program.attrLoc[2] > 0)
				glEnableVertexAttribArray(entry.program.attrLoc[2]);
		}
		else
			entry.program.attrLoc[0] = entry.program.attrLoc[1] = entry.program.attrLoc[2] = 0;


		pshaders[ShaderPair] = entry;
		CurrentShaderProgram = ShaderPair;
		CurrentProgram = entry.program.glprogid;
	}

	void ProgramShaderCache::SetMultiPSConstant4fv(unsigned int offset, const float *f, unsigned int count)
	{
		if (!s_ps_mapped_data)
		{
			glBindBuffer(GL_UNIFORM_BUFFER, s_ps_ubo);
			s_ps_mapped_data = reinterpret_cast<float*>(glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY));
			glBindBuffer(GL_UNIFORM_BUFFER, 0);

			if (!s_ps_mapped_data)
				PanicAlert("glMapBuffer");
		}

		std::copy(f, f + count * 4, s_ps_mapped_data + offset * 4);
	}

	void ProgramShaderCache::SetMultiVSConstant4fv(unsigned int offset, const float *f, unsigned int count)
	{
		if (!s_vs_mapped_data)
		{
			glBindBuffer(GL_UNIFORM_BUFFER, s_vs_ubo);
			s_vs_mapped_data = reinterpret_cast<float*>(glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY));
			glBindBuffer(GL_UNIFORM_BUFFER, 0);

			if (!s_vs_mapped_data)
				PanicAlert("glMapBuffer");
		}

		std::copy(f, f + count * 4, s_vs_mapped_data + offset * 4);
	}

	void ProgramShaderCache::FlushConstants()
	{
		if (s_ps_mapped_data)
		{
			glBindBuffer(GL_UNIFORM_BUFFER, s_ps_ubo);
			if (!glUnmapBuffer(GL_UNIFORM_BUFFER))
				PanicAlert("glUnmapBuffer");
			s_ps_mapped_data = NULL;
		}
		
		if (s_vs_mapped_data)
		{
			glBindBuffer(GL_UNIFORM_BUFFER, s_vs_ubo);
			if (!glUnmapBuffer(GL_UNIFORM_BUFFER))
				PanicAlert("glUnmapBuffer");
			s_vs_mapped_data = NULL;
		}

		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	GLuint ProgramShaderCache::GetCurrentProgram(void) { return CurrentProgram; }

	GLint ProgramShaderCache::GetAttr(int num)
	{
		return pshaders[CurrentShaderProgram].program.attrLoc[num];
	}
	PROGRAMSHADER ProgramShaderCache::GetShaderProgram(void)
	{
		return pshaders[CurrentShaderProgram].program;
	}
	void ProgramShaderCache::Init(void)
	{
		// We have to get the UBO alignment here because
		// if we generate a buffer that isn't aligned
		// then the UBO will fail.
		if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		{
			GLint Align;
			glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &Align);
		
			// We multiply by *4*4 because we need to get down to basic machine units.
			// So multiply by four to get how many floats we have from vec4s
			// Then once more to get bytes
			glGenBuffers(1, &s_ps_ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, s_ps_ubo);
			glBufferData(GL_UNIFORM_BUFFER, ROUND_UP(C_PENVCONST_END * sizeof(float) * 4, Align), NULL, GL_DYNAMIC_DRAW);
			glGenBuffers(1, &s_vs_ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, s_vs_ubo);
			glBufferData(GL_UNIFORM_BUFFER, ROUND_UP(C_VENVCONST_END * sizeof(float) * 4, Align), NULL, GL_DYNAMIC_DRAW);

			glBindBuffer(GL_UNIFORM_BUFFER, 0);
			// Now bind the buffer to the index point
			// We know PS is 0 since we have it statically set in the shader
			// Repeat for VS shader
			glBindBufferBase(GL_UNIFORM_BUFFER, 1, s_ps_ubo);
			glBindBufferBase(GL_UNIFORM_BUFFER, 2, s_vs_ubo);
		}
	}
	
	void ProgramShaderCache::Shutdown(void)
	{
		PCache::iterator iter = pshaders.begin();
		for (; iter != pshaders.end(); ++iter)
			iter->second.Destroy();
		pshaders.clear();

		// "A buffer object's mapped data store is automatically unmapped when the buffer object is deleted"
		glDeleteBuffers(1, &s_ps_ubo);
		glDeleteBuffers(1, &s_vs_ubo);
		s_ps_ubo = s_vs_ubo = 0;
		s_ps_mapped_data = s_vs_mapped_data = NULL;
	}
}

void GetProgramShaderId(PROGRAMUID *uid, GLuint _v, GLuint _p)
{
	uid->uid.vsid = _v;
	uid->uid.psid = _p;
}
