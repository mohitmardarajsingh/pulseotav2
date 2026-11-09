/** @type {import('next').NextConfig} */
const nextConfig = {
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
      {
        source: "/ota/:path*.bin",
        headers: [
          {
            key: "Cache-Control",
            value: "no-transform, public, max-age=31536000, immutable",
          },
          {
            key: "Content-Disposition",
            value: "attachment",
          },
        ],
      },
    ];
  },
};

export default nextConfig;
