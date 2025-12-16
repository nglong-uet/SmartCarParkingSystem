import { FlatList, Text } from "react-native";
import { useEffect, useState } from "react";
import { db } from "../constants/firebase";
import { ref, onValue } from "firebase/database";

export default function Logs() {
  const [logs, setLogs] = useState<any[]>([]);

  useEffect(() => {
    const logRef = ref(db, "logs");
    return onValue(logRef, (snap) => {
      if (!snap.exists()) return;
      const arr = Object.values(snap.val()).reverse();
      setLogs(arr as any[]);
    });
  }, []);

  return (
    <FlatList
      data={logs}
      renderItem={({ item }) => (
        <Text>
          {item.time} | {item.uid} | {item.action} | {item.status}
        </Text>
      )}
    />
  );
}
