import { View, Text, StyleSheet } from "react-native";
import { Colors } from "../constants/colors";

export default function StatCard({ title, value }: any) {
  return (
    <View style={styles.card} >
      <Text style={styles.title}> {title} </Text>
      <Text style={styles.value} > {value} </Text>
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    flex: 1,
    backgroundColor: Colors.card,
    padding: 16,
    borderRadius: 16,
    elevation: 3,
  },
  title: { color: Colors.subText },
  value: { fontSize: 28, fontWeight: "700", color: Colors.primary },
});
