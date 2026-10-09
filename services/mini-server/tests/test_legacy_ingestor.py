from types import SimpleNamespace
import unittest

from legacy_mqtt_ingestor import adapt_message


class LegacyEnvelopeTests(unittest.TestCase):
    def test_maps_only_the_expected_transport_topic(self):
        payload = b'{"device_id":"farm-01","data_mode":"real","timestamp":null}'
        mapped = adapt_message(SimpleNamespace(
            topic="smartfarm/farm-01/telemetry", payload=payload))
        self.assertEqual(mapped.topic, "saems/farm-01/telemetry")
        self.assertIs(mapped.payload, payload)

    def test_other_device_is_not_accepted(self):
        self.assertIsNone(adapt_message(SimpleNamespace(
            topic="smartfarm/farm-02/telemetry", payload=b"{}")))

    def test_command_topic_is_not_accepted(self):
        self.assertIsNone(adapt_message(SimpleNamespace(
            topic="smartfarm/farm-01/commands", payload=b"{}")))


if __name__ == "__main__":
    unittest.main()
