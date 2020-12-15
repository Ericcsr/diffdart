#include "dart/server/GUIWebsocketServer.hpp"

#include <chrono>

#include "dart/common/Aspect.hpp"
#include "dart/dynamics/BodyNode.hpp"
#include "dart/dynamics/BoxShape.hpp"
#include "dart/dynamics/ShapeFrame.hpp"
#include "dart/dynamics/ShapeNode.hpp"
#include "dart/dynamics/Skeleton.hpp"
#include "dart/dynamics/SphereShape.hpp"
#include "dart/math/Geometry.hpp"
#include "dart/neural/RestorableSnapshot.hpp"
#include "dart/server/RawJsonUtils.hpp"
#include "dart/simulation/World.hpp"

namespace dart {
namespace server {

GUIWebsocketServer::GUIWebsocketServer()
  : mServing(false),
    mMessagesQueued(0),
    mAutoflush(true),
    mScreenSize(Eigen::Vector2i(680, 420))
{
  mJson << "[";
}

/// This is a non-blocking call to start a websocket server on a given port
void GUIWebsocketServer::serve(int port)
{
  // Register signal and signal handler
  if (mServing)
  {
    std::cout << "Errer in GUIWebsocketServer::serve()! Already serving. "
                 "Ignoring request."
              << std::endl;
    return;
  }
  mServing = true;
  mServer = new WebsocketServer();

  // Register our network callbacks, ensuring the logic is run on the main
  // thread's event loop
  mServer->connect([&](ClientConnection conn) {
    mServerEventLoop.post([&, conn]() {
      std::clog << "Connection opened." << std::endl;
      std::clog << "There are now " << mServer->numConnections()
                << " open connections." << std::endl;
      // Send a hello message to the client
      // mServer->send(conn) seems to break, cause conn appears to get cleaned
      // up in race conditions (it's a weak pointer)
      std::stringstream json;
      json << "[";
      bool isFirst = true;
      for (auto pair : mBoxes)
      {
        if (isFirst)
          isFirst = false;
        else
          json << ",";
        encodeCreateBox(json, pair.second);
      }
      for (auto pair : mSpheres)
      {
        if (isFirst)
          isFirst = false;
        else
          json << ",";
        encodeCreateSphere(json, pair.second);
      }
      for (auto pair : mLines)
      {
        if (isFirst)
          isFirst = false;
        else
          json << ",";
        encodeCreateLine(json, pair.second);
      }
      for (auto pair : mText)
      {
        if (isFirst)
          isFirst = false;
        else
          json << ",";
        encodeCreateText(json, pair.second);
      }
      for (auto pair : mButtons)
      {
        if (isFirst)
          isFirst = false;
        else
          json << ",";
        encodeCreateButton(json, pair.second);
      }
      for (auto pair : mSliders)
      {
        if (isFirst)
          isFirst = false;
        else
          json << ",";
        encodeCreateSlider(json, pair.second);
      }
      for (auto pair : mPlots)
      {
        if (isFirst)
          isFirst = false;
        else
          json << ",";
        encodeCreatePlot(json, pair.second);
      }
      for (auto key : mMouseInteractionEnabled)
      {
        if (isFirst)
          isFirst = false;
        else
          json << ",";
        encodeEnableMouseInteraction(json, key);
      }

      json << "]";

      // std::cout << json.str() << std::endl;

      mServer->send(conn, json.str());
      // mServer->broadcast("{\"type\": 1}");
      /*
      mServer->broadcast(
          "{\"type\": \"init\", \"world\": " + mWorld->toJson() + "}");
      */

      for (auto listener : mConnectionListeners)
      {
        listener();
      }
    });
  });
  mServer->disconnect([&](ClientConnection /* conn */) {
    mServerEventLoop.post([&]() {
      std::clog << "Connection closed." << std::endl;
      std::clog << "There are now " << mServer->numConnections()
                << " open connections." << std::endl;
    });
  });
  mServer->message([&](ClientConnection /* conn */, const Json::Value& args) {
    mServerEventLoop.post([args, this]() {
      if (args["type"].asString() == "keydown")
      {
        std::string key = args["key"].asString();
        this->mKeysDown.insert(key);
        for (auto listener : this->mKeydownListeners)
        {
          listener(key);
        }
      }
      else if (args["type"].asString() == "keyup")
      {
        std::string key = args["key"].asString();
        this->mKeysDown.erase(key);
        for (auto listener : this->mKeyupListeners)
        {
          listener(key);
        }
      }
      else if (args["type"].asString() == "button_click")
      {
        std::string key = args["key"].asString();
        if (mButtons.find(key) != mButtons.end())
        {
          mButtons[key].onClick();
        }
      }
      else if (args["type"].asString() == "slider_set_value")
      {
        std::string key = args["key"].asString();
        double value = args["value"].asDouble();
        if (mSliders.find(key) != mSliders.end())
        {
          mSliders[key].value = value;
          mSliders[key].onChange(value);
        }
      }
      else if (args["type"].asString() == "screen_resize")
      {
        Eigen::Vector2i size
            = Eigen::Vector2i(args["size"][0].asInt(), args["size"][1].asInt());
        mScreenSize = size;

        for (auto handler : mScreenResizeListeners)
        {
          handler(size);
        }
      }
      else if (args["type"].asString() == "drag")
      {
        std::string key = args["key"].asString();
        Eigen::Vector3d pos = Eigen::Vector3d(
            args["pos"][0].asDouble(),
            args["pos"][1].asDouble(),
            args["pos"][2].asDouble());

        for (auto handler : mDragListeners[key])
        {
          handler(pos);
        }
      }
    });
  });

  // Start the networking thread
  mServerThread = new std::thread([&]() {
    // block signals in this thread and subsequently
    // spawned threads so they're guaranteed to go to the main thread
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

    mServer->run(port);
  });

  // Start the event loop for the main thread
  mWork = new asio::io_service::work(mServerEventLoop);

  // unblock signals in this thread
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGTERM);
  pthread_sigmask(SIG_UNBLOCK, &sigset, nullptr);

  // The signal set is used to register termination notifications
  mSignalSet = new asio::signal_set(mServerEventLoop, SIGINT, SIGTERM);

  // register the handle_stop callback
  mSignalSet->async_wait([&](asio::error_code const& error, int signal_number) {
    if (error == asio::error::operation_aborted)
    {
      std::cout << "Signal listener was terminated by asio" << std::endl;
    }
    else if (error)
    {
      std::cout << "Got an error registering termination signals: " << error
                << std::endl;
    }
    else if (
        signal_number == SIGINT || signal_number == SIGTERM
        || signal_number == SIGQUIT)
    {
      std::cout << "Shutting down the server..." << std::endl;
      stopServing();
      mServerEventLoop.stop();
      exit(signal_number);
    }
  });

  // Start a thread to handle the main server event loop
  mAsioThread = new std::thread([&] { mServerEventLoop.run(); });
}

/// This kills the server, if one was running
void GUIWebsocketServer::stopServing()
{
  if (!mServing)
    return;
  delete mWork;
  mServer->stop();
  mServerThread->join();
  delete mServer;
  delete mServerThread;
  mServing = false;
}

/// Returns true if we're serving
bool GUIWebsocketServer::isServing()
{
  return mServing;
}

/// This adds a listener that will get called when someone connects to the
/// server
void GUIWebsocketServer::registerConnectionListener(
    std::function<void()> listener)
{
  mConnectionListeners.push_back(listener);
}

/// This adds a listener that will get called when ctrl+C is pressed
void GUIWebsocketServer::registerShutdownListener(
    std::function<void()> listener)
{
  mShutdownListeners.push_back(listener);
}

/// This adds a listener that will get called when there is a key-down event
/// on the web client
void GUIWebsocketServer::registerKeydownListener(
    std::function<void(std::string)> listener)
{
  mKeydownListeners.push_back(listener);
}

/// This adds a listener that will get called when there is a key-up event
/// on the web client
void GUIWebsocketServer::registerKeyupListener(
    std::function<void(std::string)> listener)
{
  mKeyupListeners.push_back(listener);
}

/// Gets the set of all the keys currently being pressed
const std::unordered_set<std::string>& GUIWebsocketServer::getKeysDown() const
{
  return mKeysDown;
}

/// This tells us whether or not to automatically flush after each command
void GUIWebsocketServer::setAutoflush(bool autoflush)
{
  mAutoflush = autoflush;
}

/// This sends the current list of commands to the web GUI
void GUIWebsocketServer::flush()
{
  const std::lock_guard<std::mutex> lock(mJsonMutex);

  mJson << "]";
  std::string json = mJson.str();
  // std::cout << json << std::endl;
  if (mServing)
  {
    mServerEventLoop.post([json, this]() { mServer->broadcast(json); });
  }

  // Reset
  mMessagesQueued = 0;
  mJson.str(std::string());
  mJson.clear();
  mJson << "[";
}

/// This is a high-level command that creates/updates all the shapes in a
/// world by calling the lower-level commands
GUIWebsocketServer& GUIWebsocketServer::renderWorld(
    std::shared_ptr<simulation::World> world, std::string prefix)
{
  bool oldAutoflush = mAutoflush;
  mAutoflush = false;

  for (int i = 0; i < world->getNumSkeletons(); i++)
  {
    std::shared_ptr<dynamics::Skeleton> skel = world->getSkeleton(i);
    for (int j = 0; j < skel->getNumBodyNodes(); j++)
    {
      dynamics::BodyNode* node = skel->getBodyNode(j);
      std::vector<dynamics::ShapeNode*> shapeNodes
          = node->getShapeNodesWith<dynamics::VisualAspect>();
      for (int k = 0; k < shapeNodes.size(); k++)
      {
        dynamics::ShapeNode* node = shapeNodes[k];
        dynamics::VisualAspect* visual = node->getVisualAspect();
        dynamics::Shape* shape = node->getShape().get();

        std::string shapeName = prefix + "_" + skel->getName() + "_"
                                + node->getName() + "_" + std::to_string(k);

        if (!hasObject(shapeName))
        {
          // Create the object from scratch
          if (shape->getType() == "BoxShape")
          {
            dynamics::BoxShape* boxShape
                = dynamic_cast<dynamics::BoxShape*>(shape);
            createBox(
                shapeName,
                boxShape->getSize(),
                node->getWorldTransform().translation(),
                math::matrixToEulerXYZ(node->getWorldTransform().rotation()),
                visual->getColor());
          }
          else if (shape->getType() == "SphereShape")
          {
            dynamics::SphereShape* sphereShape
                = dynamic_cast<dynamics::SphereShape*>(shape);
            createSphere(
                shapeName,
                sphereShape->getRadius(),
                node->getWorldTransform().translation(),
                visual->getColor());
          }
        }
        else
        {
          // Otherwise, we just need to send updates for anything that changed
          Eigen::Vector3d pos = node->getWorldTransform().translation();
          Eigen::Vector3d euler
              = math::matrixToEulerXYZ(node->getWorldTransform().rotation());
          Eigen::Vector3d color = visual->getColor();
          // std::cout << "Color " << shapeName << ":" << color << std::endl;

          if (getObjectPosition(shapeName) != pos)
            setObjectPosition(shapeName, pos);
          if (getObjectRotation(shapeName) != euler)
            setObjectRotation(shapeName, euler);
          if (getObjectColor(shapeName) != color)
            setObjectColor(shapeName, color);
        }
      }
    }
  }

  flush();
  mAutoflush = oldAutoflush;
  return *this;
}

/// This is a high-level command that renders a given trajectory as a bunch of
/// lines in the world, one per body
GUIWebsocketServer& GUIWebsocketServer::renderTrajectoryLines(
    std::shared_ptr<simulation::World> world,
    Eigen::MatrixXd positions,
    std::string prefix)
{
  assert(positions.rows() == world->getNumDofs());

  bool oldAutoflush = mAutoflush;
  mAutoflush = false;

  std::unordered_map<std::string, std::vector<Eigen::Vector3d>> paths;
  std::unordered_map<std::string, Eigen::Vector3d> colors;

  neural::RestorableSnapshot snapshot(world);
  for (int t = 0; t < positions.cols(); t++)
  {
    world->setPositions(positions.col(t));
    for (int i = 0; i < world->getNumSkeletons(); i++)
    {
      std::shared_ptr<dynamics::Skeleton> skel = world->getSkeleton(i);
      for (int j = 0; j < skel->getNumBodyNodes(); j++)
      {
        dynamics::BodyNode* node = skel->getBodyNode(j);
        std::vector<dynamics::ShapeNode*> shapeNodes
            = node->getShapeNodesWith<dynamics::VisualAspect>();
        for (int k = 0; k < shapeNodes.size(); k++)
        {
          dynamics::ShapeNode* node = shapeNodes[k];
          dynamics::VisualAspect* visual = node->getVisualAspect();

          std::string shapeName = prefix + "_" + skel->getName() + "_"
                                  + node->getName() + "_" + std::to_string(k);
          paths[shapeName].push_back(node->getWorldTransform().translation());
          colors[shapeName] = visual->getColor();
        }
      }
    }
  }
  snapshot.restore();

  for (auto pair : paths)
  {
    // This command will automatically overwrite any lines with the same key
    createLine(pair.first, pair.second, colors[pair.first]);
  }

  flush();
  mAutoflush = oldAutoflush;
  return *this;
}

/// This completely resets the web GUI, deleting all objects, UI elements, and
/// listeners
GUIWebsocketServer& GUIWebsocketServer::clear()
{
  queueCommand(
      [&](std::stringstream& json) { json << "{ \"type\": \"clear_all\" }"; });
  mBoxes.clear();
  mLines.clear();
  mSpheres.clear();
  mText.clear();
  mButtons.clear();
  mSliders.clear();
  mPlots.clear();
  mScreenResizeListeners.clear();
  mKeydownListeners.clear();
  mShutdownListeners.clear();
  return *this;
}

/// This creates a box in the web GUI under a specified key
GUIWebsocketServer& GUIWebsocketServer::createBox(
    const std::string& key,
    const Eigen::Vector3d& size,
    const Eigen::Vector3d& pos,
    const Eigen::Vector3d& euler,
    const Eigen::Vector3d& color)
{
  Box box;
  box.key = key;
  box.size = size;
  box.pos = pos;
  box.euler = euler;
  box.color = color;

  mBoxes[key] = box;

  queueCommand([&](std::stringstream& json) { encodeCreateBox(json, box); });

  return *this;
}

/// This creates a sphere in the web GUI under a specified key
GUIWebsocketServer& GUIWebsocketServer::createSphere(
    const std::string& key,
    double radius,
    const Eigen::Vector3d& pos,
    const Eigen::Vector3d& color)
{
  Sphere sphere;
  sphere.key = key;
  sphere.radius = radius;
  sphere.pos = pos;
  sphere.color = color;

  mSpheres[key] = sphere;

  queueCommand(
      [&](std::stringstream& json) { encodeCreateSphere(json, sphere); });

  return *this;
}

/// This creates a line in the web GUI under a specified key
GUIWebsocketServer& GUIWebsocketServer::createLine(
    const std::string& key,
    const std::vector<Eigen::Vector3d>& points,
    const Eigen::Vector3d& color)
{
  Line line;
  line.key = key;
  line.points = points;
  line.color = color;

  mLines[key] = line;

  queueCommand([&](std::stringstream& json) { encodeCreateLine(json, line); });

  return *this;
}

/// This returns true if we've already got an object with the key "key"
bool GUIWebsocketServer::hasObject(const std::string& key)
{
  if (mBoxes.find(key) != mBoxes.end())
    return true;
  if (mSpheres.find(key) != mSpheres.end())
    return true;
  if (mLines.find(key) != mLines.end())
    return true;
  return false;
}

/// This returns the position of an object, if we've got it (and it's not a
/// line). Otherwise it returns Vector3d::Zero().
Eigen::Vector3d GUIWebsocketServer::getObjectPosition(const std::string& key)
{
  if (mBoxes.find(key) != mBoxes.end())
    return mBoxes[key].pos;
  if (mSpheres.find(key) != mSpheres.end())
    return mSpheres[key].pos;
  return Eigen::Vector3d::Zero();
}

/// This returns the rotation of an object, if we've got it (and it's not a
/// line or a sphere). Otherwise it returns Vector3d::Zero().
Eigen::Vector3d GUIWebsocketServer::getObjectRotation(const std::string& key)
{
  if (mBoxes.find(key) != mBoxes.end())
    return mBoxes[key].euler;
  return Eigen::Vector3d::Zero();
}

/// This returns the color of an object, if we've got it. Otherwise it returns
/// Vector3d::Zero().
Eigen::Vector3d GUIWebsocketServer::getObjectColor(const std::string& key)
{
  if (mBoxes.find(key) != mBoxes.end())
    return mBoxes[key].color;
  if (mSpheres.find(key) != mSpheres.end())
    return mSpheres[key].color;
  if (mLines.find(key) != mLines.end())
    return mLines[key].color;
  return Eigen::Vector3d::Zero();
}

/// This moves an object (e.g. box, sphere, line) to a specified position
GUIWebsocketServer& GUIWebsocketServer::setObjectPosition(
    const std::string& key, const Eigen::Vector3d& pos)
{
  if (mBoxes.find(key) != mBoxes.end())
  {
    mBoxes[key].pos = pos;
  }
  if (mSpheres.find(key) != mSpheres.end())
  {
    mSpheres[key].pos = pos;
  }

  queueCommand([&](std::stringstream& json) {
    json << "{ \"type\": \"set_object_pos\", \"key\": \"" << key
         << "\", \"pos\": ";
    vec3ToJson(json, pos);
    json << "}";
  });

  return *this;
}

/// This moves an object (e.g. box, sphere, line) to a specified orientation
GUIWebsocketServer& GUIWebsocketServer::setObjectRotation(
    const std::string& key, const Eigen::Vector3d& euler)
{
  if (mBoxes.find(key) != mBoxes.end())
  {
    mBoxes[key].euler = euler;
  }

  queueCommand([&](std::stringstream& json) {
    json << "{ \"type\": \"set_object_rotation\", \"key\": \"" << key
         << "\", \"euler\": ";
    vec3ToJson(json, euler);
    json << "}";
  });

  return *this;
}

/// This changes an object (e.g. box, sphere, line) color
GUIWebsocketServer& GUIWebsocketServer::setObjectColor(
    const std::string& key, const Eigen::Vector3d& color)
{
  if (mBoxes.find(key) != mBoxes.end())
  {
    mBoxes[key].color = color;
  }
  if (mSpheres.find(key) != mSpheres.end())
  {
    mSpheres[key].color = color;
  }
  if (mLines.find(key) != mLines.end())
  {
    mLines[key].color = color;
  }

  queueCommand([&](std::stringstream& json) {
    json << "{ \"type\": \"set_object_color\", \"key\": \"" << key
         << "\", \"color\": ";
    vec3ToJson(json, color);
    json << "}";
  });

  return *this;
}

/// This enables mouse events on an object (if they're not already), and calls
/// "listener" whenever the object is dragged with the desired drag
/// coordinates
GUIWebsocketServer& GUIWebsocketServer::registerDragListener(
    const std::string& key, std::function<void(Eigen::Vector3d)> listener)
{
  mMouseInteractionEnabled.emplace(key);
  queueCommand([&](std::stringstream& json) {
    encodeEnableMouseInteraction(json, key);
  });
  mDragListeners[key].push_back(listener);
  return *this;
}

/// This deletes an object by key
GUIWebsocketServer& GUIWebsocketServer::deleteObject(const std::string& key)
{
  mBoxes.erase(key);
  mSpheres.erase(key);
  mLines.erase(key);

  queueCommand([&](std::stringstream& json) {
    json << "{ \"type\": \"delete_object\", \"key\": \"" << key << "\" }";
  });

  return *this;
}

/// This gets the current screen size
Eigen::Vector2i GUIWebsocketServer::getScreenSize()
{
  return mScreenSize;
}

/// This registers a callback to get called whenever the screen size changes.
void GUIWebsocketServer::registerScreenResizeListener(
    std::function<void(Eigen::Vector2i)> listener)
{
  mScreenResizeListeners.push_back(listener);
}

/// This places some text on the screen at the specified coordinates
GUIWebsocketServer& GUIWebsocketServer::createText(
    const std::string& key,
    const std::string& contents,
    const Eigen::Vector2i& fromTopLeft,
    const Eigen::Vector2i& size)
{
  Text text;
  text.key = key;
  text.contents = contents;
  text.fromTopLeft = fromTopLeft;
  text.size = size;

  mText[key] = text;

  queueCommand([&](std::stringstream& json) { encodeCreateText(json, text); });

  return *this;
}

/// This changes the contents of text on the screen
GUIWebsocketServer& GUIWebsocketServer::setTextContents(
    const std::string& key, const std::string& newContents)
{
  if (mText.find(key) != mText.end())
  {
    mText[key].contents = newContents;

    queueCommand([&](std::stringstream& json) {
      json << "{ \"type\": \"set_text_contents\", \"key\": " << key
           << "\", \"label\": \"" << escapeJson(newContents) << "\" }";
    });
  }
  else
  {
    std::cout
        << "Tried to setTextContents() for a key (" << key
        << ") that doesn't exist as a Text object. Call createText() first."
        << std::endl;
  }

  return *this;
}

/// This places a clickable button on the screen at the specified coordinates
GUIWebsocketServer& GUIWebsocketServer::createButton(
    const std::string& key,
    const std::string& label,
    const Eigen::Vector2i& fromTopLeft,
    const Eigen::Vector2i& size,
    std::function<void()> onClick)
{
  Button button;
  button.key = key;
  button.label = label;
  button.fromTopLeft = fromTopLeft;
  button.size = size;
  button.onClick = onClick;

  mButtons[key] = button;

  queueCommand(
      [&](std::stringstream& json) { encodeCreateButton(json, button); });

  return *this;
}

/// This changes the contents of text on the screen
GUIWebsocketServer& GUIWebsocketServer::setButtonLabel(
    const std::string& key, const std::string& newLabel)
{
  if (mButtons.find(key) != mButtons.end())
  {
    mButtons[key].label = newLabel;

    queueCommand([&](std::stringstream& json) {
      json << "{ \"type\": \"set_button_label\", \"key\": " << key
           << "\", \"label\": \"" << escapeJson(newLabel) << "\" }";
    });
  }
  else
  {
    std::cout
        << "Tried to setButtonLabel() for a key (" << key
        << ") that doesn't exist as a Button object. Call createButton() first."
        << std::endl;
  }

  return *this;
}

/// This creates a slider
GUIWebsocketServer& GUIWebsocketServer::createSlider(
    const std::string& key,
    const Eigen::Vector2i& fromTopLeft,
    const Eigen::Vector2i& size,
    double min,
    double max,
    double value,
    bool onlyInts,
    bool horizontal,
    std::function<void(double)> onChange)
{
  Slider slider;
  slider.key = key;
  slider.fromTopLeft = fromTopLeft;
  slider.size = size;
  slider.min = min;
  slider.max = max;
  slider.value = value;
  slider.onlyInts = onlyInts;
  slider.horizontal = horizontal;
  slider.onChange = onChange;

  mSliders[key] = slider;

  queueCommand(
      [&](std::stringstream& json) { encodeCreateSlider(json, slider); });

  return *this;
}

/// This changes the contents of text on the screen
GUIWebsocketServer& GUIWebsocketServer::setSliderValue(
    const std::string& key, double value)
{
  if (mSliders.find(key) != mSliders.end())
  {
    mSliders[key].value = value;

    queueCommand([&](std::stringstream& json) {
      json << "{ \"type\": \"set_slider_value\", \"key\": " << key
           << "\", \"value\": " << value << " }";
    });
  }
  else
  {
    std::cout
        << "Tried to setSliderValue() for a key (" << key
        << ") that doesn't exist as a Slider object. Call createSlider() first."
        << std::endl;
  }

  return *this;
}

/// This changes the contents of text on the screen
GUIWebsocketServer& GUIWebsocketServer::setSliderMin(
    const std::string& key, double min)
{
  if (mSliders.find(key) != mSliders.end())
  {
    mSliders[key].min = min;

    queueCommand([&](std::stringstream& json) {
      json << "{ \"type\": \"set_slider_min\", \"key\": " << key
           << "\", \"value\": " << min << " }";
    });
  }
  else
  {
    std::cout
        << "Tried to setSliderMin() for a key (" << key
        << ") that doesn't exist as a Slider object. Call createSlider() first."
        << std::endl;
  }

  return *this;
}

/// This changes the contents of text on the screen
GUIWebsocketServer& GUIWebsocketServer::setSliderMax(
    const std::string& key, double max)
{
  if (mSliders.find(key) != mSliders.end())
  {
    mSliders[key].max = max;

    queueCommand([&](std::stringstream& json) {
      json << "{ \"type\": \"set_slider_max\", \"key\": " << key
           << "\", \"value\": " << max << " }";
    });
  }
  else
  {
    std::cout
        << "Tried to setSliderMax() for a key (" << key
        << ") that doesn't exist as a Slider object. Call createSlider() first."
        << std::endl;
  }

  return *this;
}

/// This creates a plot to display data on the GUI
GUIWebsocketServer& GUIWebsocketServer::createPlot(
    const std::string& key,
    const Eigen::Vector2i& fromTopLeft,
    const Eigen::Vector2i& size,
    const std::vector<double>& xs,
    double minX,
    double maxX,
    const std::vector<double>& ys,
    double minY,
    double maxY,
    const std::string& type)
{
  Plot plot;
  plot.key = key;
  plot.fromTopLeft = fromTopLeft;
  plot.size = size;
  plot.xs = xs;
  plot.minX = minX;
  plot.maxX = maxX;
  plot.ys = ys;
  plot.minY = minY;
  plot.maxY = maxY;
  plot.type = type;

  mPlots[key] = plot;

  queueCommand([&](std::stringstream& json) { encodeCreatePlot(json, plot); });

  return *this;
}

/// This changes the contents of a plot, along with its display limits
GUIWebsocketServer& GUIWebsocketServer::setPlotData(
    const std::string& key,
    const std::vector<double>& xs,
    double minX,
    double maxX,
    const std::vector<double>& ys,
    double minY,
    double maxY)
{
  if (mPlots.find(key) != mPlots.end())
  {
    mPlots[key].xs = xs;
    mPlots[key].minX = minX;
    mPlots[key].maxX = maxX;
    mPlots[key].ys = ys;
    mPlots[key].minY = minY;
    mPlots[key].maxY = maxY;

    queueCommand([&](std::stringstream& json) {
      json << "{ \"type\": \"set_plot_data\", \"key\": " << key
           << "\", \"xs\": ";
      vecToJson(json, xs);
      json << ", \"ys\": ";
      vecToJson(json, ys);
      json << ", \"min_x\": " << minX;
      json << ", \"max_x\": " << maxX;
      json << ", \"min_y\": " << minY;
      json << ", \"max_y\": " << maxY;
      json << " }";
    });
  }
  else
  {
    std::cout
        << "Tried to setPlotData() for a key (" << key
        << ") that doesn't exist as a Plot object. Call createPlot() first."
        << std::endl;
  }
}

/// This moves a UI element on the screen
GUIWebsocketServer& GUIWebsocketServer::setUIElementPosition(
    const std::string& key, const Eigen::Vector2i& fromTopLeft)
{
  if (mText.find(key) != mText.end())
  {
    mText[key].fromTopLeft = fromTopLeft;
  }
  if (mButtons.find(key) != mButtons.end())
  {
    mButtons[key].fromTopLeft = fromTopLeft;
  }
  if (mSliders.find(key) != mSliders.end())
  {
    mSliders[key].fromTopLeft = fromTopLeft;
  }
  if (mPlots.find(key) != mPlots.end())
  {
    mPlots[key].fromTopLeft = fromTopLeft;
  }

  queueCommand([&](std::stringstream& json) {
    json << "{ \"type\": \"set_ui_elem_pos\", \"key\": " << key
         << "\", \"from_top_left\": ";
    vec2ToJson(json, fromTopLeft);
    json << " }";
  });
  return *this;
}

/// This changes the size of a UI element
GUIWebsocketServer& GUIWebsocketServer::setUIElementSize(
    const std::string& key, const Eigen::Vector2i& size)
{
  if (mText.find(key) != mText.end())
  {
    mText[key].size = size;
  }
  if (mButtons.find(key) != mButtons.end())
  {
    mButtons[key].size = size;
  }
  if (mSliders.find(key) != mSliders.end())
  {
    mSliders[key].size = size;
  }
  if (mPlots.find(key) != mPlots.end())
  {
    mPlots[key].size = size;
  }

  queueCommand([&](std::stringstream& json) {
    json << "{ \"type\": \"set_ui_elem_size\", \"key\": " << key
         << "\", \"size\": ";
    vec2ToJson(json, size);
    json << " }";
  });
  return *this;
}

/// This deletes a UI element by key
GUIWebsocketServer& GUIWebsocketServer::deleteUIElement(const std::string& key)
{
  mText.erase(key);
  mButtons.erase(key);
  mSliders.erase(key);
  mPlots.erase(key);

  queueCommand([&](std::stringstream& json) {
    json << "{ \"type\": \"delete_ui_elem\", \"key\": \"" << key << "\" }";
  });

  return *this;
}

void GUIWebsocketServer::queueCommand(
    std::function<void(std::stringstream&)> writeCommand)
{
  const std::lock_guard<std::mutex>* lock
      = new std::lock_guard<std::mutex>(mJsonMutex);

  if (mMessagesQueued > 0)
  {
    mJson << ",";
  }
  mMessagesQueued++;
  writeCommand(mJson);

  if (mAutoflush)
  {
    // Release the lock before flushing, because that will grab the lock
    delete lock;
    flush();
  }
  else
  {
    delete lock;
  }
}

void GUIWebsocketServer::encodeCreateBox(std::stringstream& json, Box& box)
{
  json << "{ \"type\": \"create_box\", \"key\": \"" << box.key
       << "\", \"size\": ";
  vec3ToJson(json, box.size);
  json << ", \"pos\": ";
  vec3ToJson(json, box.pos);
  json << ", \"euler\": ";
  vec3ToJson(json, box.euler);
  json << ", \"color\": ";
  vec3ToJson(json, box.color);
  json << "}";
}

void GUIWebsocketServer::encodeCreateSphere(
    std::stringstream& json, Sphere& sphere)
{
  json << "{ \"type\": \"create_sphere\", \"key\": \"" << sphere.key
       << "\", \"radius\": " << sphere.radius;
  json << ", \"pos\": ";
  vec3ToJson(json, sphere.pos);
  json << ", \"color\": ";
  vec3ToJson(json, sphere.color);
  json << "}";
}

void GUIWebsocketServer::encodeCreateLine(std::stringstream& json, Line& line)
{
  json << "{ \"type\": \"create_line\", \"key\": \"" << line.key;
  json << "\", \"points\": [";
  bool firstPoint = true;
  for (Eigen::Vector3d& point : line.points)
  {
    if (firstPoint)
      firstPoint = false;
    else
      json << ", ";
    vec3ToJson(json, point);
  }
  json << "], \"color\": ";
  vec3ToJson(json, line.color);
  json << "}";
}

void GUIWebsocketServer::encodeEnableMouseInteraction(
    std::stringstream& json, const std::string& key)
{
  json << "{ \"type\": \"enable_mouse\", \"key\": \"" << key << "\" }";
}

void GUIWebsocketServer::encodeCreateText(std::stringstream& json, Text& text)
{
  json << "{ \"type\": \"create_text\", \"key\": \"" << text.key
       << "\", \"from_top_left\": ";
  vec2ToJson(json, text.fromTopLeft);
  json << ", \"size\": ";
  vec2ToJson(json, text.size);
  json << ", \"contents\": \"" << escapeJson(text.contents);
  json << "\" }";
}

void GUIWebsocketServer::encodeCreateButton(
    std::stringstream& json, Button& button)
{
  json << "{ \"type\": \"create_button\", \"key\": \"" << button.key
       << "\", \"from_top_left\": ";
  vec2ToJson(json, button.fromTopLeft);
  json << ", \"size\": ";
  vec2ToJson(json, button.size);
  json << ", \"label\": \"" << escapeJson(button.label);
  json << "\" }";
}

void GUIWebsocketServer::encodeCreateSlider(
    std::stringstream& json, Slider& slider)
{
  json << "{ \"type\": \"create_slider\", \"key\": \"" << slider.key
       << "\", \"from_top_left\": ";
  vec2ToJson(json, slider.fromTopLeft);
  json << ", \"size\": ";
  vec2ToJson(json, slider.size);
  json << ", \"max\": " << slider.max;
  json << ", \"min\": " << slider.min;
  json << ", \"value\": " << slider.value;
  json << ", \"only_ints\": " << slider.onlyInts ? "true" : "false";
  json << ", \"horizontal\": " << slider.horizontal ? "true" : "false";
  json << "}";
}

void GUIWebsocketServer::encodeCreatePlot(std::stringstream& json, Plot& plot)
{
  json << "{ \"type\": \"create_plot\", \"key\": \"" << plot.key
       << "\", \"from_top_left\": ";
  vec2ToJson(json, plot.fromTopLeft);
  json << ", \"size\": ";
  vec2ToJson(json, plot.size);
  json << ", \"max_x\": " << plot.maxX;
  json << ", \"min_x\": " << plot.minX;
  json << ", \"max_y\": " << plot.maxY;
  json << ", \"min_y\": " << plot.minY;
  json << ", \"xs\": ";
  vecToJson(json, plot.xs);
  json << ", \"ys\": ";
  vecToJson(json, plot.ys);
  json << ", \"plot_type\": \"" << plot.type;
  json << "\" }";
}

} // namespace server
} // namespace dart