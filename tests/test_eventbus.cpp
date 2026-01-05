#include "EventBus.h"
#include "Logger.h"
#include "test_utils.h"

TEST(EventBus_SingleSubscriberPublish) {
  resetEventBus();
  EventCapture<UpdateEvent> capture;

  UpdateEvent event{16.67f, 42};
  EventBus::instance().publish(event);

  capture.assertCount(1);
  assert(capture.first().deltaTime == 16.67f);
  assert(capture.first().frameNumber == 42);

  resetEventBus();
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

TEST(EventCapture_Assertions) {
  resetEventBus();
  EventCapture<UpdateEvent> updates;

  // Publish multiple events
  EventBus::instance().publish(UpdateEvent{16.67f, 1});
  EventBus::instance().publish(UpdateEvent{16.67f, 2});
  EventBus::instance().publish(UpdateEvent{16.67f, 3});

  // Test count assertions
  updates.assertCount(3);
  updates.assertAtLeast(2);

  // Test access methods
  assert(updates.first().frameNumber == 1);
  assert(updates.last().frameNumber == 3);
  assert(updates.at(1).frameNumber == 2);

  // Test assertAny
  updates.assertAny([](const UpdateEvent& e) { return e.frameNumber == 2; });

  // Test assertAll
  updates.assertAll([](const UpdateEvent& e) { return e.deltaTime == 16.67f; });

  // Test countMatching
  assert(updates.countMatching(
             [](const UpdateEvent& e) { return e.frameNumber > 1; }) == 2);

  resetEventBus();
}

int main() {
  Logger::init();

  test_EventBus_SingleSubscriberPublish();
  test_EventBus_MultipleSubscribers();
  test_EventBus_MultipleEventTypes();
  test_EventBus_PublishWithNoSubscribers();
  test_EventBus_Clear();
  test_EventCapture_Assertions();

  return 0;
}
