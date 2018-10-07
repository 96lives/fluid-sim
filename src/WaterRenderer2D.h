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
        HMM_Vec2(0.0f, 0.0f),
        HMM_Vec2(0.0f, 1.0f),
        HMM_Vec2(1.0f, 1.0f),
        HMM_Vec2(0.0f, 0.0f),
        HMM_Vec2(1.0f, 1.0f),
        HMM_Vec2(1.0f, 0.0f),
};

class WaterRenderer2D {
private:
    static constexpr int SIZEX = WaterSimSettings::Dim2D::SIZEX;
    static constexpr int SIZEY = WaterSimSettings::Dim2D::SIZEY;

    GLuint waterCellVAO;
    GLuint solidCellVAO;
    GLuint pressureCellVAO;
    GLuint phiCellVAO;
    GLuint particleVAO;

    GLuint quadVBO;
    GLuint waterCellOffsetVBO;
    GLuint solidCellOffsetVBO;
    GLuint pressureCellOffsetVBO;
    GLuint pressureCellValueVBO;
    GLuint allCellOffsetVBO;
    GLuint phiCellValueVBO;
    GLuint particleVBO;

    hmm_vec2 quadVertices[6];
    StackVec<hmm_vec2, SIZEX*SIZEY> waterCellLocations = {};
    StackVec<hmm_vec2, SIZEX*SIZEY> solidCellLocations = {};
    StackVec<hmm_vec2, SIZEX*SIZEY> pressureCellLocations = {};
    StackVec<float, SIZEX*SIZEY> pressureCellValues = {};
    StackVec<hmm_vec2, SIZEX*SIZEY> allCellLocations = {};
    StackVec<float, SIZEX*SIZEY> phiCellValues = {};

    StackVec<hmm_vec2, 4*SIZEX*SIZEY> particleLocations = {};

    Shader cellShader;
    Shader particleShader;
    Shader cellFieldShader;

    WaterSim2D* sim;

    bool renderParticles = true;
    bool renderCells = false;
    bool renderPressures = false;
    bool renderLevelSet = true;

    const char* particleVS = R"SHADER(
#version 330 core

layout (location = 0) in vec2 inPos;

out vec4 color;

uniform mat4 view;
uniform mat4 proj;

void main() {
    gl_Position = proj * view * vec4(inPos, 0.2, 1.0);
    color = vec4(1.0, 1.0, 1.0, 1.0);
}

)SHADER";

    const char* cellVS = R"SHADER(
#version 330 core

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inOffset;

out vec4 color;

uniform vec4 uniColor;
uniform mat4 view;
uniform mat4 proj;

void main() {
    gl_Position = proj * view * vec4(inPos + inOffset, 0.0, 1.0);
    color = uniColor;
}
)SHADER";

    const char* cellFieldVS = R"SHADER(
#version 330 core

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inOffset;
layout (location = 2) in float inValue;

out vec4 color;

uniform mat4 view;
uniform mat4 proj;
uniform vec4 posColor;
uniform vec4 negColor;

void main() {
    gl_Position = proj * view * vec4(inPos + inOffset, 0.1, 1.0);
    color = inValue * posColor + (1 - inValue) * negColor;
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
        particleShader = Shader::fromStr(particleVS, cellFS);
        cellFieldShader = Shader::fromStr(cellFieldVS, cellFS);

        memcpy(quadVertices, origQuadVertices, sizeof(origQuadVertices));
        for (int i = 0; i < 6*36; i++) {
            quadVertices[i] *= sim->dx;
        }

        allCellLocations.size = SIZEX*SIZEY;
        sim->iterate([&](size_t i, size_t j) {
            allCellLocations[j*SIZEX + i] = HMM_Vec2(i,j) * sim->dx;
        });

        updateBuffers();

        glGenVertexArrays(1, &waterCellVAO);
        glGenVertexArrays(1, &solidCellVAO);
        glGenVertexArrays(1, &pressureCellVAO);
        glGenVertexArrays(1, &phiCellVAO);
        glGenVertexArrays(1, &particleVAO);
        glGenBuffers(1, &quadVBO);
        glGenBuffers(1, &waterCellOffsetVBO);
        glGenBuffers(1, &solidCellOffsetVBO);
        glGenBuffers(1, &pressureCellOffsetVBO);
        glGenBuffers(1, &pressureCellValueVBO);
        glGenBuffers(1, &allCellOffsetVBO);
        glGenBuffers(1, &phiCellValueVBO);
        glGenBuffers(1, &particleVBO);

        glBindVertexArray(waterCellVAO);

        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(hmm_vec2) * 6, quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(hmm_vec2), 0);

        glBindBuffer(GL_ARRAY_BUFFER, waterCellOffsetVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(hmm_vec2) * SIZEX*SIZEY, waterCellLocations.data, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(hmm_vec2), 0);
        glVertexAttribDivisor(1, 1);

        glBindVertexArray(solidCellVAO);

        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(hmm_vec2), 0);

        glBindBuffer(GL_ARRAY_BUFFER, solidCellOffsetVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(hmm_vec2) * SIZEX*SIZEY, solidCellLocations.data, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(hmm_vec2), 0);
        glVertexAttribDivisor(1, 1);

        glBindVertexArray(pressureCellVAO);

        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(hmm_vec2), 0);

        glBindBuffer(GL_ARRAY_BUFFER, pressureCellOffsetVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(hmm_vec2) * SIZEX*SIZEY, pressureCellLocations.data, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(hmm_vec2), 0);
        glVertexAttribDivisor(1, 1);

        glBindBuffer(GL_ARRAY_BUFFER, pressureCellValueVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * SIZEX*SIZEY, pressureCellValues.data, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), 0);
        glVertexAttribDivisor(2, 1);

        glBindVertexArray(phiCellVAO);

        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(hmm_vec2), 0);

        glBindBuffer(GL_ARRAY_BUFFER, allCellOffsetVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(hmm_vec2) * SIZEX*SIZEY, allCellLocations.data, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(hmm_vec2), 0);
        glVertexAttribDivisor(1, 1);

        glBindBuffer(GL_ARRAY_BUFFER, phiCellValueVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * SIZEX*SIZEY, phiCellValues.data, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), 0);
        glVertexAttribDivisor(2, 1);

        glBindVertexArray(particleVAO);

        glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(hmm_vec2) * 4*SIZEX*SIZEY, particleLocations.data, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(hmm_vec2), 0);

        glBindVertexArray(0);

        camera->addShader(&cellShader);
        camera->addShader(&particleShader);
        camera->addShader(&cellFieldShader);
    }

    void update() {
        if (!sim->rendered) {
            updateBuffers();
            if (renderCells) {
                glBindVertexArray(waterCellVAO);
                glBindBuffer(GL_ARRAY_BUFFER, waterCellOffsetVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(hmm_vec2) * waterCellLocations.size, waterCellLocations.data);
                glBindVertexArray(solidCellVAO);
                glBindBuffer(GL_ARRAY_BUFFER, solidCellOffsetVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(hmm_vec2) * solidCellLocations.size, solidCellLocations.data);
                glBindVertexArray(0);
            }
            if (renderPressures) {
                glBindVertexArray(pressureCellVAO);
                glBindBuffer(GL_ARRAY_BUFFER, pressureCellOffsetVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(hmm_vec2) * pressureCellLocations.size, pressureCellLocations.data);
                glBindVertexArray(particleVAO);
                glBindBuffer(GL_ARRAY_BUFFER, pressureCellValueVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * pressureCellValues.size, pressureCellValues.data);
                glBindVertexArray(0);
            }
            if (renderParticles) {
                glBindVertexArray(particleVAO);
                glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(hmm_vec2) * particleLocations.size, particleLocations.data);
                glBindVertexArray(0);
            }
            if (renderLevelSet) {
                glBindVertexArray(phiCellVAO);
                glBindBuffer(GL_ARRAY_BUFFER, phiCellValueVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * phiCellValues.size, phiCellValues.data);
                glBindVertexArray(0);
            }
        }
    }

    void draw() {
        if (renderCells) {
            cellShader.use();
            cellShader.setVector4("uniColor", HMM_Vec4(0.0f, 0.0f, 1.0f, 1.0f));
            glBindVertexArray(waterCellVAO);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, waterCellLocations.size);
            cellShader.setVector4("uniColor", HMM_Vec4(0.5f, 0.5f, 0.5f, 1.0f));
            glBindVertexArray(solidCellVAO);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, solidCellLocations.size);
            glBindVertexArray(0);
        }
        if (renderPressures) {
            cellFieldShader.use();
            cellFieldShader.setVector4("posColor", HMM_Vec4(1.0f, 0.0f, 0.0f, 1.0f));
            cellFieldShader.setVector4("negColor", HMM_Vec4(0.0f, 0.0f, 1.0f, 1.0f));
            glBindVertexArray(pressureCellVAO);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, pressureCellLocations.size);
            glBindVertexArray(0);
        }

        if (renderParticles) {
            particleShader.use();
            glBindVertexArray(particleVAO);
            glDrawArrays(GL_POINTS, 0, particleLocations.size);
            glBindVertexArray(0);
        }

        if (renderLevelSet) {
            cellFieldShader.use();
            cellFieldShader.setVector4("posColor", HMM_Vec4(1.0f, 0.0f, 0.0f, 1.0f));
            cellFieldShader.setVector4("negColor", HMM_Vec4(0.0f, 0.0f, 1.0f, 1.0f));
            glBindVertexArray(phiCellVAO);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, allCellLocations.size);
            glBindVertexArray(0);
        }

    }

    void drawUI() {
        ImGui::Begin("Water Simulation");
        ImGui::Text("Current frame: %f", sim->currentTime);
        ImGui::Text("Current stage: %s", sim->printStage());
        ImGui::Text("Average pressure: %f", sim->avgPressure());
        ImGui::Text("Average pressure in fluid: %f", sim->avgPressureInFluid());
        ImGui::Checkbox("Render cells", &renderCells);
        ImGui::Checkbox("Render pressures", &renderPressures);
        ImGui::Checkbox("Render particles", &renderParticles);
        ImGui::Checkbox("Render level set", &renderLevelSet);

        ImGui::End();
    }

    void updateBuffers() {
        if (renderCells) {
            waterCellLocations.size = 0;
            solidCellLocations.size = 0;
            sim->iterate([&](size_t i, size_t j) {
                WaterSim2D::CellType cellType = sim->cell(i,j);
                if (cellType == WaterSim2D::CellType::FLUID) {
                    waterCellLocations.push(HMM_Vec2((float)i,(float)j) * sim->dx);
                }
                else if (cellType == WaterSim2D::CellType::SOLID) {
                    solidCellLocations.push(HMM_Vec2((float)i,(float)j) * sim->dx);
                }
            });
        }
        if (renderPressures) {
            pressureCellLocations.size = 0;
            pressureCellValues.size = 0;
            sim->iterate([&](size_t i, size_t j) {
                if (sim->p(i,j) != 0) {
                    pressureCellLocations.push(HMM_Vec2((float)i,(float)j) * sim->dx);
                    float p = (float)sim->p(i,j);
                    pressureCellValues.push(utils::sigmoid(0.01f * p));
                }
            });
        }
        if (renderParticles) {
            particleLocations.size = sim->particles.size;
            for (size_t i = 0; i < sim->particles.size; i++) {
                particleLocations[i].X = (float)sim->particles[i].x;
                particleLocations[i].Y = (float)sim->particles[i].y;
            }
        }
        phiCellValues.size = 0;
        if (renderLevelSet) {
            sim->iterate([&](size_t i, size_t j) {
                phiCellValues.push(utils::sigmoid<float>(100.f * sim->phi(i,j)));
            });
        }
    }

    /*
    void createWaterMesh() {
        float dx = (float)sim->dx;
        StackVec<hmm_vec2, 4> contourLookupTable[16] = {};
        contourLookupTable[1].push(HMM_Vec2(0, 0.5f));
        contourLookupTable[1].push(HMM_Vec2(0.5f, 1));
        contourLookupTable[2].push(HMM_Vec2(0.5f, 1));
        contourLookupTable[2].push(HMM_Vec2(1, 0.5f));
        contourLookupTable[3].push(HMM_Vec2(0, 0.5f));
        contourLookupTable[3].push(HMM_Vec2(1, 0.5f));
        contourLookupTable[4].push(HMM_Vec2(0.5f, 0));
        contourLookupTable[4].push(HMM_Vec2(1, 0.5f));
        contourLookupTable[5].push(HMM_Vec2(0, 0.5f));
        contourLookupTable[5].push(HMM_Vec2(0.5f, 0));
        contourLookupTable[5].push(HMM_Vec2(0.5f, 1));
        contourLookupTable[5].push(HMM_Vec2(1, 0.5f));
        contourLookupTable[6].push(HMM_Vec2(0.5f, 0));
        contourLookupTable[6].push(HMM_Vec2(0.5f, 1));
        contourLookupTable[7].push(HMM_Vec2(0, 0.5f));
        contourLookupTable[7].push(HMM_Vec2(0.5f, 0));
        contourLookupTable[8].push(HMM_Vec2(0, 0.5f));
        contourLookupTable[8].push(HMM_Vec2(0.5f, 0));
        contourLookupTable[9].push(HMM_Vec2(0.5f, 0));
        contourLookupTable[9].push(HMM_Vec2(0.5f, 1));
        contourLookupTable[10].push(HMM_Vec2(0.5f, 1));
        contourLookupTable[10].push(HMM_Vec2(0.5f, 1));
        contourLookupTable[10].push(HMM_Vec2(0.5f, 1));
        contourLookupTable[10].push(HMM_Vec2(0.5f, 1));

        auto phiSign = new Array2D<bool, SIZEX, SIZEY>();
        defer {delete phiSign;};
        sim->iterate([&](size_t i, size_t j) {
            (*phiSign)(i,j) = sim->phi(i,j) > 0;
        });

        for (size_t j = 0; j < SIZEY - 1; j++) {
            for (size_t i = 0; i < SIZEX - 1; i++) {
                int key = ((*phiSign)(i+1,j+1) * 8) + ((*phiSign)(i,j+1) * 4)
                        + ((*phiSign)(i+1,j) * 2) + ((*phiSign)(i,j));
                switch (key) {
                    case 0:

                }

            }
        }
    }
     */
};

#endif //FLUID_SIM_WATERRENDERER2D_H
