import { View, Text, StyleSheet } from "react-native";
import { Colors } from "../constants/colors";
import { MaterialIcons } from "@expo/vector-icons";

type Props = {
  slot: number;
  occupied: boolean;
  uid?: string;
};

export default function ParkingSlot({ slot, occupied, uid }: Props) {
  return (
    <View
      style={[
        styles.slot,
        { borderColor: occupied ? Colors.success : Colors.danger },
      ]}
    >
      <View style={styles.header}>
        <Text style={styles.slotText}>Slot {slot}</Text>
        <View
          style={[
            styles.indicator,
            { backgroundColor: occupied ? Colors.success : Colors.danger },
          ]}
        />
      </View>

      {occupied ? (
        <>
          <MaterialIcons
            name="directions-car"
            size={40}
            color={Colors.success}
          />
          <Text style={styles.uid}>UID: {uid}</Text>
        </>
      ) : (
        <Text style={styles.empty}>TRỐNG</Text>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  slot: {
    width: "45%",
    backgroundColor: Colors.card,
    borderRadius: 16,
    borderWidth: 2,
    padding: 16,
    alignItems: "center",
  },
  header: {
    width: "100%",
    flexDirection: "row",
    justifyContent: "space-between",
    marginBottom: 12,
  },
  slotText: {
    fontWeight: "700",
  },
  indicator: {
    width: 14,
    height: 14,
    borderRadius: 7,
  },
  uid: {
    marginTop: 8,
    fontSize: 12,
    color: Colors.subText,
  },
  empty: {
    color: Colors.subText,
    marginTop: 12,
  },
});
