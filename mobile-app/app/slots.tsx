import { View, StyleSheet, Text } from "react-native";
import { useEffect, useState } from "react";
import { db } from "../constants/firebase";
import { ref, onValue } from "firebase/database";
import ParkingSlot from "../components/ParkingSlot";
import { Colors } from "../constants/colors";

const TOTAL_SLOTS = 4;

export default function SlotsScreen() {
  const [cars, setCars] = useState<any[]>([]);

  useEffect(() => {
    const insideRef = ref(db, "parking/inside");

    return onValue(insideRef, (snap) => {
      if (!snap.exists()) {
        setCars([]);
        return;
      }

      const data = snap.val();
      const arr = Object.keys(data).map((uid) => ({
        uid,
        time: data[uid].time,
      }));

      setCars(arr);
    });
  }, []);

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Bãi đỗ xe</Text>

      <View style={styles.grid}>
        {Array.from({ length: TOTAL_SLOTS }).map((_, i) => {
          const car = cars[i];
          return (
            <ParkingSlot
              key={i}
              slot={i + 1}
              occupied={!!car}
              uid={car?.uid}
            />
          );
        })}
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