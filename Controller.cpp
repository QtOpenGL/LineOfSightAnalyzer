#include "Controller.h"
#include "Constants.h"
#include "Window.h"

#include <QApplication>
#include <QDebug>

Controller::Controller(QObject *parent)
    : QObject(parent)
    , mWidth(DEFUALT_SCREEN_WIDTH)
    , mHeight(DEFUALT_SCREEN_HEIGHT)
    , mDevicePixelRatio(1.0f)
    , mActiveCamera(nullptr)
    , mMinElevation(0)
    , mMaxElevation(1000)
    , mMaxDistance(1000.0f)
    , mObserverHeight(45.0f)
    , mLockObserver(false)
    , mDebugEnabled(false)
    , mBias(0.005f)
    , mColorAttachments{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}
{
    mCamera3D = new FreeCamera;
    mCamera3D->setPosition(QVector3D(0, 1000, 0));
    mCamera3D->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), -90));
    mCamera3D->setHorizontalFov(50.0f);
    mCamera3D->setZNear(1.0f);
    mCamera3D->setZFar(1000000.0f);
    mCamera3D->resize(mWidth, mHeight);

    mCamera2D = new OrthographicCamera;
    mCamera2D->setPosition(QVector3D(0, 100000, 0));
    mCamera2D->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), -90));
    mCamera2D->setZNear(1.0f);
    mCamera2D->setZFar(1000000.0f);
    mCamera2D->resize(mWidth, mHeight);
    setActiveCamera(mCamera2D);

    mFBOFormats.insert(FramebufferType::Terrain, new QOpenGLFramebufferObjectFormat);
    mFBOFormats[FramebufferType::Terrain]->setAttachment(QOpenGLFramebufferObject::Depth);
    mFBOFormats[FramebufferType::Terrain]->setSamples(0);
    mFBOFormats[FramebufferType::Terrain]->setTextureTarget(GL_TEXTURE_2D);
}

Controller::~Controller() {}

void Controller::init()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    QString path = ":/Resources/HeightMaps/00.png";
    QImage image(path);

    if (image.isNull())
        qWarning() << "An image at " + path + " is null.";

    mHeightMap = new QOpenGLTexture(image, QOpenGLTexture::GenerateMipMaps);
    mHeightMap->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
    mHeightMap->setMinMagFilters(QOpenGLTexture::Filter::LinearMipMapLinear, QOpenGLTexture::Filter::Linear);

    mTerrain = new Terrain(mHeightMap->width(), mHeightMap->height());

    mQuad = new Quad;

    mShaderManager = ShaderManager::instance();
    mShaderManager->init();

    // Line of sight stuff
    {
        for (int i = 0; i < 6; i++)
        {
            mObservers << new FreeCamera;
            mObservers[i]->setPosition(QVector3D(0, 100, 0));
            mObservers[i]->resize(OBSERVER_FBO_WIDTH, OBSERVER_FBO_HEIGHT);
            mObservers[i]->setHorizontalFov(90.0f);
            mObservers[i]->setZNear(10.0f);
            mObservers[i]->setZFar(1000.0f);
        }
        auto rollFix = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), 180);

        mObservers[0]->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), -90) * rollFix);
        mObservers[1]->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), 90) * rollFix);
        mObservers[2]->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 90));
        mObservers[3]->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), -90));
        mObservers[4]->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), 180) * rollFix);
        mObservers[5]->setRotation(rollFix);

        glGenFramebuffers(1, &mObserverFBO);
        glGenTextures(1, &mObserverFBODepthMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, mObserverFBODepthMap);
        for (unsigned int i = 0; i < 6; ++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, OBSERVER_FBO_WIDTH, OBSERVER_FBO_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, mObserverFBO);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mObserverFBODepthMap, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    createFramebuffers();
}

void Controller::render(float ifps)
{
    mDevicePixelRatio = mWindow->devicePixelRatioF();

    mIfps = ifps;
    mActiveCamera->update(mIfps);
    mActiveCamera->setDevicePixelRatio(mDevicePixelRatio);

    // Update observer position
    for (int i = 0; i < 6; i++)
    {
        if (!mLockObserver)
            mObservers[i]->setPosition(mMouseWorldPosition + QVector3D(0, mObserverHeight, 0));

        if (mDebugEnabled)
            mObservers[i]->setPosition(mCamera3D->position());

        mObservers[i]->setZFar(mMaxDistance);
    }

    // Observer
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mObserverFBO);
        glViewport(0, 0, OBSERVER_FBO_WIDTH, OBSERVER_FBO_HEIGHT);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        mShaderManager->bind(ShaderType::ObserverShader);

        for (int i = 0; i < 6; i++)
        {
            mShaderManager->setUniformValue(QString("VPs[%1]").arg(i), mObservers[i]->getViewProjectionMatrix());
        }

        mShaderManager->setUniformValue("minElevation", mMinElevation);
        mShaderManager->setUniformValue("maxElevation", mMaxElevation);
        mShaderManager->setSampler("heightMap", 0, mHeightMap->textureId());
        mShaderManager->setUniformValue("observerPosition", mObservers[0]->position());
        mShaderManager->setUniformValue("farPlane", mObservers[0]->getZFar());
        mTerrain->render();
        mShaderManager->release();
    }

    // DEBUG
    if (mDebugEnabled)
    {
        QOpenGLFramebufferObject::bindDefault();
        glViewport(0, 0, mWidth, mHeight);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mShaderManager->bind(ShaderType::DebugShader);
        mShaderManager->setUniformValue("IVP", mCamera3D->getRotationMatrix().inverted() * mCamera3D->getProjectionMatrix().inverted());
        mShaderManager->setSampler("depthMap", 1, mObserverFBODepthMap, GL_TEXTURE_CUBE_MAP);
        mQuad->render();
        mShaderManager->release();
    }

    // Terrain
    if (!mDebugEnabled)
    {
        mFBOs[FramebufferType::Terrain]->bind();
        glViewport(0, 0, mWidth, mHeight);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        mShaderManager->bind(ShaderType::TerrainShader);
        mShaderManager->setUniformValue("VP", mActiveCamera->getViewProjectionMatrix());
        mShaderManager->setUniformValue("minElevation", mMinElevation);
        mShaderManager->setUniformValue("maxElevation", mMaxElevation);
        mShaderManager->setUniformValue("maxDistance", mMaxDistance);
        mShaderManager->setSampler("heightMap", 0, mHeightMap->textureId());
        mShaderManager->setSampler("depthMap", 1, mObserverFBODepthMap, GL_TEXTURE_CUBE_MAP);
        mShaderManager->setUniformValue("observerPosition", mObservers[0]->position());
        mShaderManager->setUniformValue("farPlane", mObservers[0]->getZFar());
        mShaderManager->setUniformValue("maxDistance", mMaxDistance);
        mShaderManager->setUniformValue("bias", mBias);
        mTerrain->render();
        mShaderManager->release();

        QOpenGLFramebufferObject::bindDefault();
        glViewport(0, 0, mWidth, mHeight);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        QOpenGLFramebufferObject::blitFramebuffer(nullptr, //
                                                  QRect(0, 0, mWidth, mHeight),
                                                  mFBOs[FramebufferType::Terrain],
                                                  QRect(0, 0, mFBOs[FramebufferType::Terrain]->width(), mFBOs[FramebufferType::Terrain]->height()),
                                                  GL_COLOR_BUFFER_BIT,
                                                  GL_NEAREST,
                                                  0,
                                                  0);
    }

    // ImGui Stuff
    QtImGui::newFrame();
    //ImGui::ShowDemoWindow();
    drawGUI();
    ImGui::Render();
    QtImGui::render();
}

void Controller::drawGUI()
{
    auto viewDir = mActiveCamera->getViewDirection();

    ImGui::SetNextWindowSize(ImVec2(420, 820), ImGuiCond_FirstUseEver);
    ImGui::Begin("Controls");

    // Render Settings
    if (!ImGui::CollapsingHeader("Render Settings"))
    {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Observer");
        ImGui::SliderFloat("Max Distance##ObserverSettings", &mMaxDistance, 1, 5000);
        ImGui::SliderFloat("Height##ObserverSettings", &mObserverHeight, 0, 1000);

        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Elevation");
        ImGui::SliderFloat("Min Elevation##RenderSettings", &mMinElevation, 1, 1000);
        ImGui::SliderFloat("Max Elevation##RenderSettings", &mMaxElevation, mMinElevation, 10000);

        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Debug");
        ImGui::SliderFloat("Bias##RenderSettings", &mBias, 0, 0.1);
        ImGui::Checkbox("Debug", &mDebugEnabled);
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Info");
    ImGui::Text("Observer Position: (%.1f, %.1f,%.1f)", mObservers[0]->position().x(), mObservers[0]->position().y(), mObservers[0]->position().z());
    ImGui::Text("Mouse World Position: (%.1f, %.1f, %.1f)", mMouseWorldPosition[0], mMouseWorldPosition[1], mMouseWorldPosition[2]);
    ImGui::Text("Camera View Direction: (%.1f, %.1f, %.1f)", viewDir.x(), viewDir.y(), viewDir.z());
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Hints");
    ImGui::Text("Observer Lock / Unlock: L");
    ImGui::Text("Go to Observer's Position: G");
    ImGui::Text("Camera Speed Fast: Shift");
    ImGui::Text("Camera Speed Very Fast: Ctrl");

    ImGui::End();
}

void Controller::setActiveCamera(Camera *newActiveCamera)
{
    if (newActiveCamera == mActiveCamera)
        return;

    if (newActiveCamera)
        newActiveCamera->setActive(true);

    if (mActiveCamera)
        mActiveCamera->setActive(false);

    mActiveCamera = newActiveCamera;
}

void Controller::setWindow(Window *newWindow)
{
    mWindow = newWindow;
}

void Controller::wheelMoved(QWheelEvent *event)
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    mActiveCamera->wheelMoved(event);
}

void Controller::mousePressed(QMouseEvent *event)
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    mActiveCamera->mousePressed(event);
}

void Controller::mouseReleased(QMouseEvent *event)
{
    mActiveCamera->mouseReleased(event);
}

void Controller::mouseMoved(QMouseEvent *event)
{
    mMouseMoving = true;
    QTimer::singleShot(100, [=]() { mMouseMoving = false; });

    if (ImGui::GetIO().WantCaptureMouse)
        return;

    mFBOs[FramebufferType::Terrain]->bind();
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glReadPixels(event->position().x() * mDevicePixelRatio, mFBOs[FramebufferType::Terrain]->height() - event->position().y() * mDevicePixelRatio, 1, 1, GL_RGB, GL_FLOAT, &mMouseWorldPosition);
    mFBOs[FramebufferType::Terrain]->release();

    mActiveCamera->mouseMoved(event);
}

void Controller::keyPressed(QKeyEvent *event)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
        return;

    if (event->key() == Qt::Key_1)
        setActiveCamera(mCamera3D);
    else if (event->key() == Qt::Key_2)
        setActiveCamera(mCamera2D);
    else if (event->key() == Qt::Key_L)
        mLockObserver = !mLockObserver;
    else if (event->key() == Qt::Key_G)
    {
        mCamera3D->setPosition(mObservers[0]->position());
        mCamera3D->resize(mWidth, mHeight);
        setActiveCamera(mCamera3D);
    } else
        mActiveCamera->keyPressed(event);
}

void Controller::keyReleased(QKeyEvent *event)
{
    mActiveCamera->keyReleased(event);
}

void Controller::resize(int width, int height)
{
    mWindow->makeCurrent();

    mWidth = width * mDevicePixelRatio;
    mHeight = height * mDevicePixelRatio;

    deleteFramebuffers();
    createFramebuffers();

    mActiveCamera->resize(mWidth, mHeight);

    mWindow->doneCurrent();
}

void Controller::deleteFramebuffers()
{
    auto keys = mFBOs.keys();

    for (auto type : keys)
        if (mFBOs[type])
            delete mFBOs[type];
}

void Controller::createFramebuffers()
{
    mFBOs.insert(FramebufferType::Terrain, new QOpenGLFramebufferObject(mWidth, mHeight, *mFBOFormats[FramebufferType::Terrain]));
    mFBOs[FramebufferType::Terrain]->addColorAttachment(mWidth, mHeight, GL_RGB32F);

    mFBOs[FramebufferType::Terrain]->bind();
    glDrawBuffers(2, mColorAttachments);
    mFBOs[FramebufferType::Terrain]->release();
}
