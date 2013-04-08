#include "Input.h"

#include <deque>
#include <string.h>
#include <ppapi/cpp/var.h>
#include <pthread.h>

namespace love {
namespace window {
namespace ppapi {

void ConvertEvent(const pp::InputEvent& in_event, InputEvent* out_event) {
  InputType type;
  switch (in_event.GetType()) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      type = INPUT_MOUSE;
      out_event->mouse.type = MOUSE_DOWN;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEUP:
      type = INPUT_MOUSE;
      out_event->mouse.type = MOUSE_UP;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      type = INPUT_MOUSE;
      out_event->mouse.type = MOUSE_MOVE;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
      type = INPUT_MOUSE;
      out_event->mouse.type = MOUSE_ENTER;
      break;
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      type = INPUT_MOUSE;
      out_event->mouse.type = MOUSE_LEAVE;
      break;
    case PP_INPUTEVENT_TYPE_WHEEL:
      type = INPUT_WHEEL;
      break;
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
      type = INPUT_KEY;
      out_event->key.type = RAW_KEY_DOWN;
      break;
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      type = INPUT_KEY;
      out_event->key.type = KEY_DOWN;
      break;
    case PP_INPUTEVENT_TYPE_KEYUP:
      type = INPUT_KEY;
      out_event->key.type = KEY_UP;
      break;
    case PP_INPUTEVENT_TYPE_CHAR:
      type = INPUT_CHARACTER;
      break;
  }

  out_event->type = type;
  out_event->modifiers = in_event.GetModifiers();
  switch (type) {
    case INPUT_MOUSE: {
      pp::MouseInputEvent mouse_event(in_event);
      out_event->mouse.x = mouse_event.GetPosition().x();
      out_event->mouse.y = mouse_event.GetPosition().y();
      out_event->mouse.movement_x = mouse_event.GetMovement().x();
      out_event->mouse.movement_y = mouse_event.GetMovement().y();
      switch (mouse_event.GetButton()) {
        case PP_INPUTEVENT_MOUSEBUTTON_NONE:
          out_event->mouse.button = MOUSE_NONE;
          break;
        case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
          out_event->mouse.button = MOUSE_LEFT;
          break;
        case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
          out_event->mouse.button = MOUSE_MIDDLE;
          break;
        case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
          out_event->mouse.button = MOUSE_RIGHT;
          break;
      }
      break;
    }
    case INPUT_WHEEL: {
      pp::WheelInputEvent wheel_event(in_event);
      out_event->wheel.delta_x = wheel_event.GetDelta().x();
      out_event->wheel.delta_y = wheel_event.GetDelta().y();
      out_event->wheel.ticks_x = wheel_event.GetTicks().x();
      out_event->wheel.ticks_y = wheel_event.GetTicks().y();
      out_event->wheel.scroll_by_page = wheel_event.GetScrollByPage();
      break;
    }
    case INPUT_KEY: {
      pp::KeyboardInputEvent key_event(in_event);
      out_event->key.code = key_event.GetKeyCode();
      break;
    }
    case INPUT_CHARACTER: {
      pp::KeyboardInputEvent key_event(in_event);
      strncpy(out_event->character.text,
              key_event.GetCharacterText().AsString().c_str(),
              5);
      break;
    }
  }
}

typedef std::deque<InputEvent> InputEventQueue;
InputEventQueue g_InputEventQueue;
pthread_mutex_t g_EventQueueMutex;
pthread_cond_t g_QueueNonEmpty;

void UpdateInputState(const InputEvent& event);

void InitializeEventQueue() {
  pthread_mutex_init(&g_EventQueueMutex, NULL);
  pthread_cond_init(&g_QueueNonEmpty, NULL);
}

void EnqueueEvent(const pp::InputEvent& event) {
  InputEvent converted_event;
  ConvertEvent(event, &converted_event);

  UpdateInputState(converted_event);

  pthread_mutex_lock(&g_EventQueueMutex);
  g_InputEventQueue.push_back(converted_event);
  pthread_cond_signal(&g_QueueNonEmpty);
  pthread_mutex_unlock(&g_EventQueueMutex);
}

bool DequeueEvent(InputEvent* out_event) {
  bool has_event = false;
  pthread_mutex_lock(&g_EventQueueMutex);
  if (!g_InputEventQueue.empty()) {
    *out_event = g_InputEventQueue.front();
    g_InputEventQueue.pop_front();
    has_event = true;
  }
  pthread_mutex_unlock(&g_EventQueueMutex);
  return has_event;
}

void DequeueAllEvents(InputEvents* out_events) {
  pthread_mutex_lock(&g_EventQueueMutex);
  out_events->resize(g_InputEventQueue.size());
  std::copy(g_InputEventQueue.begin(), g_InputEventQueue.end(),
            out_events->begin());
  g_InputEventQueue.clear();
  pthread_mutex_unlock(&g_EventQueueMutex);
}

void WaitForEvent() {
  pthread_mutex_lock(&g_EventQueueMutex);
  while (g_InputEventQueue.empty()) {
    pthread_cond_wait(&g_QueueNonEmpty, &g_EventQueueMutex);
  }
  pthread_mutex_unlock(&g_EventQueueMutex);
}

int g_MouseX;
int g_MouseY;
bool g_MouseButton[MOUSE_BUTTON_MAX];

int GetMouseX() {
  return g_MouseX;
}

int GetMouseY() {
  return g_MouseY;
}

bool IsMouseButtonPressed(MouseButton button) {
  if (button <= MOUSE_NONE || button >= MOUSE_BUTTON_MAX)
    return false;
  return g_MouseButton[button];
}

bool g_Keys[KEY_CODE_MAX];

bool IsKeyPressed(uint32_t code) {
  if (code >= KEY_CODE_MAX)
    return false;
  return g_Keys[code];
}

void UpdateInputState(const InputEvent& event) {
  switch (event.type) {
    case INPUT_MOUSE:
      g_MouseX = event.mouse.x;
      g_MouseY = event.mouse.y;

      switch (event.mouse.type) {
        case MOUSE_DOWN:
          g_MouseButton[event.mouse.button] = true;
          break;
        case MOUSE_UP:
          g_MouseButton[event.mouse.button] = false;
          break;
      }
      break;

    case INPUT_KEY:
      switch (event.key.type) {
        case KEY_DOWN:
          if (event.key.code < KEY_CODE_MAX)
            g_Keys[event.key.code] = true;
          break;
        case KEY_UP:
          if (event.key.code < KEY_CODE_MAX)
            g_Keys[event.key.code] = false;
          break;
      }
      break;
  }
}

}  // ppapi
}  // window
}  // love