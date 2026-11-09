/** @type {import('next').NextConfig} */
const nextConfig = {
  async redirects() {
    return [
      {
        source: '/ota/firmware_:version.bin',
        destination: 'https://github.com/mohitmardarajsingh/pulseotav2/releases/download/pulseotav2binaries/firmware_:version.bin',
        permanent: false,
      },
    ];
  },
  async headers() {
    return [
      {
        source: "/ota/version.json",
        headers: [
          {
            key: "Cache-Control",
            value: "no-store, no-cache, must-revalidate, proxy-revalidate, max-age=0",
          },
        ],
      },
    ];
  },
};

export default nextConfig;
