#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class Shader {
public:
    GLuint ID = 0;

    Shader() = default;
    Shader(const char* vertPath, const char* fragPath);

    void use() const;
    void setBool (const std::string& name, bool  value) const;
    void setInt  (const std::string& name, int   value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3 (const std::string& name, const glm::vec3& v) const;
    void setMat4 (const std::string& name, const glm::mat4& m) const;

private:
    static GLuint compileShader(GLenum type, const std::string& src);
    static std::string readFile(const char* path);
};
