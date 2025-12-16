import { Tabs } from "expo-router";

export default function Layout() {
  return (
    <Tabs>
      <Tabs.Screen name="index" options={{ title: "Dashboard" }} />
      <Tabs.Screen name="slots" options={{ title: "Parking" }} />
      <Tabs.Screen name="logs" options={{ title: "Logs" }} />
    </Tabs>
  );
}
