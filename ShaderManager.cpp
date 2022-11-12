#include "ShaderManager.h"
#include "Shader.h"

ShaderManager::ShaderManager()
    : mActiveShader(ShaderType::None)
{}

Shader *ShaderManager::getShader(ShaderType shader)
{
    return mShaders.value(shader);
}

bool ShaderManager::init()
{
    // Terrain Shader
    {
        Shader *shader = new Shader(ShaderType::TerrainShader);
        mShaders.insert(shader->type(), shader);

        shader->addPath(QOpenGLShader::Vertex, ":/Resources/Shaders/Terrain.vert");
        shader->addPath(QOpenGLShader::Fragment, ":/Resources/Shaders/Terrain.frag");

        shader->addUniform("VP");
        shader->addUniform("minElevation");
        shader->addUniform("maxElevation");
        shader->addUniform("heightMap");
        shader->addUniform("depthMap");
        shader->addUniform("observerPosition");
        shader->addUniform("farPlane");
        shader->addUniform("maxDistance");

        shader->addAttribute("position");
        shader->addAttribute("textureCoords");

        if (!shader->init())
            return false;
    }

    // Observer Shader
    {
        Shader *shader = new Shader(ShaderType::ObserverShader);
        mShaders.insert(shader->type(), shader);

        shader->addPath(QOpenGLShader::Vertex, ":/Resources/Shaders/Observer.vert");
        shader->addPath(QOpenGLShader::Geometry, ":/Resources/Shaders/Observer.geom");
        shader->addPath(QOpenGLShader::Fragment, ":/Resources/Shaders/Observer.frag");

        shader->setUniformArray("VPs[%1]", 6);
        shader->addUniform("minElevation");
        shader->addUniform("maxElevation");
        shader->addUniform("heightMap");
        shader->addUniform("observerPosition");
        shader->addUniform("farPlane");

        shader->addAttribute("position");
        shader->addAttribute("textureCoords");

        if (!shader->init())
            return false;
    }

    // Debug Shader
    {
        Shader *shader = new Shader(ShaderType::DebugShader);
        mShaders.insert(shader->type(), shader);

        shader->addPath(QOpenGLShader::Vertex, ":/Resources/Shaders/Debug.vert");
        shader->addPath(QOpenGLShader::Fragment, ":/Resources/Shaders/Debug.frag");

        shader->addUniform("IVP");
        shader->addUniform("depthMap");

        shader->addAttribute("position");

        if (!shader->init())
            return false;
    }

    return true;
}

bool ShaderManager::bind(ShaderType shader)
{
    mActiveShader = shader;
    return mShaders.value(mActiveShader)->bind();
}

void ShaderManager::release()
{
    mShaders.value(mActiveShader)->release();
}

void ShaderManager::setUniformValue(const QString &name, int value)
{
    mShaders.value(mActiveShader)->setUniformValue(name, value);
}

void ShaderManager::setUniformValue(const QString &name, unsigned int value)
{
    mShaders.value(mActiveShader)->setUniformValue(name, value);
}

void ShaderManager::setUniformValue(const QString &name, float value)
{
    mShaders.value(mActiveShader)->setUniformValue(name, value);
}

void ShaderManager::setUniformValue(const QString &name, const QVector3D &value)
{
    mShaders.value(mActiveShader)->setUniformValue(name, value);
}

void ShaderManager::setUniformValue(const QString &name, const QVector4D &value)
{
    mShaders.value(mActiveShader)->setUniformValue(name, value);
}

void ShaderManager::setUniformValue(const QString &name, const QMatrix4x4 &value)
{
    mShaders.value(mActiveShader)->setUniformValue(name, value);
}

void ShaderManager::setUniformValue(const QString &name, const QMatrix3x3 &value)
{
    mShaders.value(mActiveShader)->setUniformValue(name, value);
}

void ShaderManager::setUniformValueArray(const QString &name, const QVector<QVector3D> &values)
{
    mShaders.value(mActiveShader)->setUniformValueArray(name, values);
}

void ShaderManager::setSampler(const QString &name, unsigned int unit, unsigned int id, GLenum target)
{
    mShaders.value(mActiveShader)->setSampler(name, unit, id, target);
}

ShaderManager *ShaderManager::instance()
{
    static ShaderManager instance;

    return &instance;
}
