import { View, Text, StyleSheet } from "react-native";
import { Colors } from "../constants/colors";

export default function SlotCard({ slot, uid, time }: any) {
  return (
    <View style={styles.card} >
      <Text style={styles.slot}>🅿 Slot {slot} </Text>
      < Text > UID: {uid} </Text>
      < Text > Vào lúc: {time} </ Text>
      < Text style={styles.status} > ĐANG ĐỖ </ Text>
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    margin: 12,
    padding: 16,
    backgroundColor: Colors.card,
    borderRadius: 14,
    elevation: 2,
  },
  slot: { fontSize: 18, fontWeight: "600" },
  status: { color: Colors.success, marginTop: 8 },
});
