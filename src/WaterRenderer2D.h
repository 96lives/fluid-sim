//
// Created by lasagnaphil on 10/2/18.
//
#ifndef FLUID_SIM_WATERRENDERER2D_H
#define FLUID_SIM_WATERRENDERER2D_H

#include <glad/glad.h>
#include <HandmadeMath.h>
#include <StackVec.h>
#include <imgui.h>

#include "WaterSimSettings.h"
#include "Shader.h"
#include "WaterSim2D.h"
#include "FirstPersonCamera.h"
#include "InputManager.h"
#include "Camera2D.h"

static hmm_vec2 origQuadVertices[6] = {
        HMM_Vec2(-0.5f, -0.5f),
        HMM_Vec2(-0.5f, 0.5f),
        HMM_Vec2(0.5f, 0.5f),
        HMM_Vec2(-0.5f, -0.5f),
        HMM_Vec2(0.5f, 0.5f),
        HMM_Vec2(0.5f, -0.5f)
};

class WaterRenderer2D {
private:
    static constexpr int SIZEX = WaterSimSettings::Dim2D::SIZEX;
    static constexpr int SIZEY = WaterSimSettings::Dim2D::SIZEY;
    static constexpr int ENABLE_DEBUG_UI = WaterSimSettings::Dim2D::ENABLE_DEBUG_UI;

    static constexpr float CELL_SIZE = 10.0f / SIZEY;
    static constexpr float VEL_LINE_SCALE = 10.0f / SIZEY;

    GLuint cellVAO;
    GLuint quadVBO;
    GLuint cellColorVBO;
    GLuint cellOffsetVBO;

    hmm_vec2 cellVertices[SIZEX*SIZEY];
    hmm_vec2 quadVertices[6];
    StackVec<hmm_vec2, SIZEX*SIZEY> waterVoxelLocations = {};

    Shader lineShader;
    Shader cellShader;

    WaterSim2D* sim;


    const char* lineVS = R"SHADER(
#version 330 core

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec4 inColor;

out vec4 color;

uniform mat4 view;
uniform mat4 proj;

void main() {
    gl_Position = proj * view * vec4(inPos, 0.0, 1.0);
    color = inColor;
}

)SHADER";

    const char* cellVS = R"SHADER(
#version 330 core

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inOffset;

out vec4 color;

uniform mat4 view;
uniform mat4 proj;

void main() {
    gl_Position = proj * view * vec4(inPos + inOffset, 0.0, 1.0);
    color = vec4(0.0, 0.0, 1.0, 1.0);
}
)SHADER";

    const char* cellFS = R"SHADER(
#version 330 core

in vec4 color;

out vec4 fragColor;

void main() {
	fragColor = color;
}
)SHADER";


public:
    void setup(WaterSim2D* sim, Camera2D* camera) {
        this->sim = sim;

        const auto EMPTY_COLOR = HMM_Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        const auto FLUID_COLOR = HMM_Vec4(0.0f, 0.0f, 1.0f, 1.0f);
        const auto SOLID_COLOR = HMM_Vec4(0.1f, 0.1f, 0.1f, 1.0f);

        cellShader = Shader::fromStr(cellVS, cellFS);

        memcpy(quadVertices, origQuadVertices, sizeof(origQuadVertices));
        for (int i = 0; i < 6*36; i++) {
            quadVertices[i] *= CELL_SIZE;
        }

        /*
        sim->mac.iterate([&](size_t i, size_t j) {
            WaterSim2D::CellType cellType = sim->cell(i, j);
            if (cellType == WaterSim2D::CellType::EMPTY) {
                cellColors[j * SIZEX + i] = EMPTY_COLOR;
            }
            else if (cellType == WaterSim2D::CellType::FLUID) {
                cellColors[j * SIZEX + i] = FLUID_COLOR;
            }
            else if (cellType == WaterSim2D::CellType::SOLID) {
                cellColors[j * SIZEX + i] = SOLID_COLOR;
            }
        });
         */

        updateWaterCellLocations();

        glGenVertexArrays(1, &cellVAO);
        glGenBuffers(1, &quadVBO);
        glGenBuffers(1, &cellOffsetVBO);

        glBindVertexArray(cellVAO);

        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(hmm_vec2) * 6, quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(hmm_vec2), 0);

        glBindBuffer(GL_ARRAY_BUFFER, cellOffsetVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(hmm_vec2) * waterVoxelLocations.size, waterVoxelLocations.data, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(hmm_vec2), 0);
        glVertexAttribDivisor(1, 1);

        glBindVertexArray(0);

        cellShader.use();
        camera->addShader(&cellShader);
    }

    void update() {
        if (!sim->rendered) {
            updateWaterCellLocations();
            glBindVertexArray(cellVAO);
            glBindBuffer(GL_ARRAY_BUFFER, cellOffsetVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(hmm_vec2) * waterVoxelLocations.size, waterVoxelLocations.data);
            glBindVertexArray(0);
        }
    }

    void draw() {
        cellShader.use();
        glBindVertexArray(cellVAO);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, waterVoxelLocations.size);
        glBindVertexArray(0);
    }

    void drawUI() {
        if (ENABLE_DEBUG_UI) {
            ImGui::Begin("Simulation Data");
            if (ImGui::CollapsingHeader("Pressure")) {
                ImGui::Columns(SIZEY, "table_p");
                ImGui::Separator();
                for (size_t j = 0; j < SIZEY; j++) {
                    for (size_t i = 0; i < SIZEX; i++) {
                        char fstr[32];
                        sprintf(fstr, "%6f", sim->p(i, SIZEY - 1 - j));
                        ImGui::Text(fstr);
                        ImGui::NextColumn();
                    }
                    ImGui::Separator();
                }
                ImGui::Columns(1);
                ImGui::Separator();
            }
            if (ImGui::CollapsingHeader("Velocity")) {
                ImGui::Columns(SIZEY, "table_vel");
                ImGui::Separator();
                for (size_t j = 0; j < SIZEY; j++) {
                    for (size_t i = 0; i < SIZEX; i++) {
                        char fstr[32];
                        Vector2d v = sim->mac.vel(i, SIZEY - 1 - j);
                        sprintf(fstr, "%2.2f %2.2f %2.2f", v.x, v.y);
                        ImGui::Text(fstr);
                        ImGui::NextColumn();
                    }
                    ImGui::Separator();
                }
                ImGui::Columns(1);
                ImGui::Separator();
            }
            ImGui::End();
        }
    }

    void updateWaterCellLocations() {
        waterVoxelLocations.size = 0;
        sim->mac.iterate([&](size_t i, size_t j) {
            WaterSim2D::CellType cellType = sim->cell(i,j);
            if (cellType == WaterSim2D::CellType::FLUID) {
                waterVoxelLocations.push(HMM_Vec2((float)i/CELL_SIZE,(float)j/CELL_SIZE));
            }
        });
    }
};

#endif //FLUID_SIM_WATERRENDERER2D_H