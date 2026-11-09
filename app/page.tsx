"use client";

import { useEffect, useState } from "react";

interface VersionData {
  version: string;
  url: string;
  notes: string;
}

export default function Home() {
  const [data, setData] = useState<VersionData | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  
  const [copiedUrl, setCopiedUrl] = useState(false);
  const [copiedEndpoint, setCopiedEndpoint] = useState(false);
  const [copiedCurl, setCopiedCurl] = useState(false);

  const endpointUrl = "https://esp32update.vercel.app/ota/version.json";
  const curlCommand = `curl ${endpointUrl}`;

  useEffect(() => {
    // Unregister any active service workers for this origin
    if (typeof window !== "undefined" && "serviceWorker" in navigator) {
      navigator.serviceWorker.getRegistrations().then((registrations) => {
        for (const registration of registrations) {
          registration.unregister();
        }
      }).catch((err) => {
        console.error("Failed to unregister service worker:", err);
      });
    }

    // Fetch version data from public static file with cache-busting timestamp
    fetch(`/ota/version.json?t=${Date.now()}`)
      .then((res) => {
        if (!res.ok) {
          throw new Error("Failed to fetch version.json from /ota/version.json");
        }
        return res.json();
      })
      .then((json: VersionData) => {
        setData(json);
        setLoading(false);
      })
      .catch((err: Error) => {
        setError(err.message);
        setLoading(false);
      });
  }, []);

  const copyToClipboard = (text: string, setCopied: (v: boolean) => void) => {
    if (typeof navigator !== "undefined" && navigator.clipboard) {
      navigator.clipboard.writeText(text).then(() => {
        setCopied(true);
        setTimeout(() => setCopied(false), 2000);
      });
    }
  };

  const ClipboardIcon = () => (
    <svg
      xmlns="http://www.w3.org/2000/svg"
      fill="none"
      viewBox="0 0 24 24"
      strokeWidth={1.5}
      stroke="currentColor"
      className="w-4 h-4"
    >
      <path
        strokeLinecap="round"
        strokeLinejoin="round"
        d="M8.25 7.5V6.108c0-1.135.845-2.098 1.976-2.192.373-.03.748-.057 1.123-.08M15.75 18H18a2.25 2.25 0 0 0 2.25-2.25V6.108c0-1.135-.845-2.098-1.976-2.192a48.424 48.424 0 0 0-1.123-.08M15.75 18.75v-1.875a3.375 3.375 0 0 0-3.375-3.375h-1.5a1.125 1.125 0 0 1-1.125-1.125v-1.5A3.375 3.375 0 0 0 6.375 7.5H5.25m11.9-3.664A2.251 2.251 0 0 0 15 2.25h-3a2.251 2.251 0 0 0-2.15 1.586m5.8 0c.065.21.1.433.1.664v.75h-6V4.5c0-.231.035-.454.1-.664M6.75 7.5H4.875c-.621 0-1.125.504-1.125 1.125v9.75c0 .621.504 1.125 1.125 1.125h9.75c.621 0 1.125-.504 1.125-1.125V16.5"
      />
    </svg>
  );

  const CheckIcon = () => (
    <svg
      xmlns="http://www.w3.org/2000/svg"
      fill="none"
      viewBox="0 0 24 24"
      strokeWidth={1.5}
      stroke="currentColor"
      className="w-4 h-4 text-[#22c55e]"
    >
      <path strokeLinecap="round" strokeLinejoin="round" d="m4.5 12.75 6 6 9-13.5" />
    </svg>
  );

  return (
    <div className="max-w-6xl mx-auto px-4 py-8 md:py-16 w-full flex-grow flex flex-col justify-between">
      {/* Header */}
      <header className="flex flex-col sm:flex-row sm:items-center sm:justify-between border-b border-[#222222] pb-6 mb-8 md:mb-12">
        <div>
          <h1 className="font-mono text-3xl font-bold tracking-tight text-[#f5f5f5] mb-1">
            PulseOTA
          </h1>
          <p className="text-sm text-gray-400">ESP32 OTA Update Server</p>
        </div>
        <div className="mt-4 sm:mt-0 flex items-center gap-2 bg-[#111111] border border-[#222222] px-3.5 py-1.5 rounded-full w-fit">
          <span className="relative flex h-2.5 w-2.5">
            <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-[#22c55e] opacity-75"></span>
            <span className="relative inline-flex rounded-full h-2.5 w-2.5 bg-[#22c55e]"></span>
          </span>
          <span className="text-xs font-medium tracking-wide text-gray-300">LIVE</span>
        </div>
      </header>

      {/* Main Content */}
      <main className="max-w-2xl mx-auto w-full flex flex-col gap-8 mb-12">
        {/* Current Version Card */}
        <section className="bg-[#111111] border border-[#222222] rounded-xl p-6 flex flex-col gap-5">
          <h2 className="text-base font-semibold text-gray-200 border-b border-[#222222] pb-3">
            Current Firmware
          </h2>
          
          {loading ? (
            <div className="py-8 flex items-center justify-center">
              <div className="animate-spin rounded-full h-5 w-5 border-b-2 border-[#3b82f6]"></div>
            </div>
          ) : error ? (
            <div className="text-sm text-red-400 py-4 font-mono">
              Error loading version: {error}
            </div>
          ) : data ? (
            <div className="flex flex-col gap-4">
              <div>
                <div className="text-xs text-gray-400 uppercase tracking-wider mb-1 font-semibold">
                  Current Version
                </div>
                <div className="font-mono text-lg font-bold text-[#22c55e] bg-[#22c55e]/10 border border-[#22c55e]/20 px-2 py-0.5 rounded w-fit">
                  v{data.version}
                </div>
              </div>

              <div>
                <div className="text-xs text-gray-400 uppercase tracking-wider mb-1.5 font-semibold">
                  Firmware URL
                </div>
                <div className="flex items-center gap-2">
                  <a
                    href={data.url}
                    target="_blank"
                    rel="noopener noreferrer"
                    className="font-mono text-sm text-[#3b82f6] hover:underline break-all bg-[#0a0a0a] border border-[#222222] px-3 py-2 rounded-lg flex-grow"
                  >
                    {data.url}
                  </a>
                  <button
                    onClick={() => copyToClipboard(data.url, setCopiedUrl)}
                    className="bg-[#111111] hover:bg-[#1c1c1c] border border-[#222222] p-2.5 rounded-lg text-gray-400 hover:text-white transition-all flex-shrink-0"
                    title="Copy URL"
                  >
                    {copiedUrl ? <CheckIcon /> : <ClipboardIcon />}
                  </button>
                </div>
              </div>

              <div>
                <div className="text-xs text-gray-400 uppercase tracking-wider mb-1 font-semibold">
                  Release Notes
                </div>
                <div className="bg-[#0a0a0a] border border-[#222222] px-3 py-2 rounded-lg text-sm text-gray-300">
                  {data.notes || "No notes available for this release."}
                </div>
              </div>
            </div>
          ) : (
            <div className="text-sm text-gray-400 py-4">No version data found.</div>
          )}
        </section>

        {/* ESP32 Check Endpoint Card */}
        <section className="bg-[#111111] border border-[#222222] rounded-xl p-6 flex flex-col gap-4">
          <h2 className="text-base font-semibold text-gray-200 border-b border-[#222222] pb-3">
            ESP32 Check Endpoint
          </h2>

          <div className="flex flex-col gap-4">
            <div>
              <div className="flex items-center justify-between mb-1.5">
                <span className="text-xs text-gray-400 uppercase tracking-wider font-semibold">
                  Polling URL
                </span>
                <button
                  onClick={() => copyToClipboard(endpointUrl, setCopiedEndpoint)}
                  className="text-xs text-[#3b82f6] hover:underline flex items-center gap-1 font-medium"
                >
                  {copiedEndpoint ? "Copied" : "Copy"}
                </button>
              </div>
              <div className="font-mono text-xs text-gray-300 bg-[#0a0a0a] border border-[#222222] px-3 py-2.5 rounded-lg break-all">
                {endpointUrl}
              </div>
            </div>

            <div>
              <div className="flex items-center justify-between mb-1.5">
                <span className="text-xs text-gray-400 uppercase tracking-wider font-semibold">
                  Test Command
                </span>
                <button
                  onClick={() => copyToClipboard(curlCommand, setCopiedCurl)}
                  className="text-xs text-[#3b82f6] hover:underline flex items-center gap-1 font-medium"
                >
                  {copiedCurl ? "Copied" : "Copy"}
                </button>
              </div>
              <div className="font-mono text-xs text-gray-300 bg-[#0a0a0a] border border-[#222222] px-3 py-2.5 rounded-lg break-all">
                {curlCommand}
              </div>
            </div>
          </div>
        </section>
      </main>

      {/* Footer */}
      <footer className="border-t border-[#222222] pt-6 text-center flex flex-col sm:flex-row sm:items-center sm:justify-between gap-3 text-xs text-gray-400">
        <div>
          PulseOTA by{" "}
          <a
            href="https://krajtilak.in"
            target="_blank"
            rel="noopener noreferrer"
            className="text-[#3b82f6] hover:underline"
          >
            krajtilak.in
          </a>
        </div>
        <div className="font-mono text-[10px] text-gray-500">
          Static Filesystem-based Updates &bull; Powered by Vercel
        </div>
      </footer>
    </div>
  );
}
