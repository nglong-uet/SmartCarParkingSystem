import { View, StyleSheet, Text } from "react-native";
import { useEffect, useState } from "react";
import { db } from "../constants/firebase";
import { ref, onValue } from "firebase/database";
import ParkingSlot from "../components/ParkingSlot";
import { Colors } from "../constants/colors";

type SlotData = {
  occupied: boolean;
  uid?: string;
  time?: string;
};

export default function SlotsScreen() {
  const [slots, setSlots] = useState<Record<string, SlotData>>({});

  useEffect(() => {
    const slotRef = ref(db, "parking/slots");

    return onValue(slotRef, (snap) => {
      if (snap.exists()) {
        setSlots(snap.val());
      }
    });
  }, []);

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Bãi đỗ xe</Text>

      <View style={styles.grid}>
        {Object.entries(slots).map(([key, slot]) => {
          const slotNumber = Number(key.replace("slot", ""));
          return (
            <ParkingSlot
              key={key}
              slot={slotNumber}
              occupied={slot.occupied}
              uid={slot.uid}
            />
          );
        })
        }
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 16,
    backgroundColor: Colors.bg,
  },
  title: {
    fontSize: 22,
    fontWeight: "700",
    marginBottom: 16,
  },
  grid: {
    flexDirection: "row",
    flexWrap: "wrap",
    gap: 12,
    justifyContent: "space-between",
  },
});
