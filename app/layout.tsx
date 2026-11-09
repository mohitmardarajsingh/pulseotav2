import type { Metadata } from "next";
import { Inter, JetBrains_Mono } from "next/font/google";
import "./globals.css";

const inter = Inter({
  subsets: ["latin"],
  variable: "--font-sans",
});

const jetbrainsMono = JetBrains_Mono({
  subsets: ["latin"],
  variable: "--font-mono",
});

export const metadata: Metadata = {
  title: "PulseOTA — ESP32 OTA Update Server",
  description: "A secure, minimal, filesystem-based OTA firmware update server for ESP32 microcontrollers.",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en" className="dark">
      <body
        className={`${inter.variable} ${jetbrainsMono.variable} font-sans bg-[#0a0a0a] text-[#f5f5f5] antialiased min-h-screen flex flex-col`}
      >
        {children}
      </body>
    </html>
  );
}
