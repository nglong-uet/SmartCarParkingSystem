import { View, Text, StyleSheet } from "react-native";
import { Colors } from "../constants/colors";
import { useEffect, useState } from "react";
import { db } from "../constants/firebase";
import { ref, onValue } from "firebase/database";
import StatCard from "../components/StatCard";

export default function Dashboard() {
  const [insideCount, setInsideCount] = useState(0);

  useEffect(() => {
    const insideRef = ref(db, "parking/inside");
    return onValue(insideRef, (snap) => {
      setInsideCount(snap.exists() ? Object.keys(snap.val()).length : 0);
    });
  }, []);

  return (
    <View style={styles.container}>
      <Text style={styles.title}>🚗 Smart Parking</Text>

      <View style={styles.row}>
        <StatCard title="Xe đang đỗ" value={insideCount} />
        <StatCard title="Chỗ trống" value={4 - insideCount} />
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { padding: 16, backgroundColor: Colors.bg, flex: 1 },
  title: { fontSize: 24, fontWeight: "700", marginBottom: 16 },
  row: { flexDirection: "row", gap: 12 },
});
