import { initializeApp } from "firebase/app";
import { getDatabase } from "firebase/database";

const firebaseConfig = {
  apiKey: "AIzaSyBBDd6RMSbYOniIWalNi5MUHo2ACG2b_mo",
  authDomain: "smartparkingsystem-f8fdb.firebaseapp.com",
  databaseURL:
    "https://smartparkingsystem-f8fdb-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "smartparkingsystem-f8fdb",
  storageBucket: "smartparkingsystem-f8fdb.appspot.com",
};

const app = initializeApp(firebaseConfig);
export const db = getDatabase(app);
