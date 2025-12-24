#include "EventBus.h"
#include "Logger.h"
#include "test_utils.h"

TEST(EventBus_SingleSubscriberPublish) {
  EventBus::instance().clear();
  int callCount = 0;

  EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
    callCount++;
    assert(e.deltaTime == 16.67f);
    assert(e.frameNumber == 42);
  });

  UpdateEvent event{16.67f, 42};
  EventBus::instance().publish(event);

  assert(callCount == 1);
  EventBus::instance().clear();
}

TEST(EventBus_MultipleSubscribers) {
  EventBus::instance().clear();
  int call1 = 0, call2 = 0, call3 = 0;

  EventBus::instance().subscribe<UpdateEvent>(
      [&](const UpdateEvent&) { call1++; });
  EventBus::instance().subscribe<UpdateEvent>(
      [&](const UpdateEvent&) { call2++; });
  EventBus::instance().subscribe<UpdateEvent>(
      [&](const UpdateEvent&) { call3++; });

  UpdateEvent event{16.67f, 1};
  EventBus::instance().publish(event);

  assert(call1 == 1);
  assert(call2 == 1);
  assert(call3 == 1);
  EventBus::instance().clear();
}

TEST(EventBus_MultipleEventTypes) {
  EventBus::instance().clear();
  int updateCalls = 0, renderCalls = 0;

  EventBus::instance().subscribe<UpdateEvent>(
      [&](const UpdateEvent&) { updateCalls++; });
  EventBus::instance().subscribe<RenderEvent>(
      [&](const RenderEvent&) { renderCalls++; });

  EventBus::instance().publish(UpdateEvent{16.67f, 1});
  EventBus::instance().publish(RenderEvent{0.5f});

  assert(updateCalls == 1);
  assert(renderCalls == 1);
  EventBus::instance().clear();
}

TEST(EventBus_PublishWithNoSubscribers) {
  EventBus::instance().clear();
  EventBus::instance().publish(UpdateEvent{16.67f, 1});
}

TEST(EventBus_Clear) {
  EventBus::instance().clear();
  int callCount = 0;

  EventBus::instance().subscribe<UpdateEvent>(
      [&](const UpdateEvent&) { callCount++; });
  EventBus::instance().publish(UpdateEvent{16.67f, 1});
  assert(callCount == 1);

  EventBus::instance().clear();
  EventBus::instance().publish(UpdateEvent{16.67f, 2});
  assert(callCount == 1);
}

int main() {
  Logger::init();

  test_EventBus_SingleSubscriberPublish();
  test_EventBus_MultipleSubscribers();
  test_EventBus_MultipleEventTypes();
  test_EventBus_PublishWithNoSubscribers();
  test_EventBus_Clear();

  return 0;
}
