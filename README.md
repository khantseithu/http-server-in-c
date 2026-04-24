# C HTTP/HTTPS Server 🔒

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)

A lightweight, production-ready HTTP/1.1 server written in C with TLS/SSL support. Ideal for embedded systems, education, or as a foundation for web services.

**Features**:

- ✅ HTTPS with modern TLS 1.3/1.2
- 🔑 Certificate-based authentication
- 🚀 Multi-client concurrency (fork-based)
- 🛡️ Security headers (HSTS, CSP, X-Content-Type)
- 📁 Static file serving & directory listings

---

## 📦 Installation

### Prerequisites

- C compiler (`gcc`/`clang`)
- OpenSSL 3.x
- GNU Make

```bash
# Clone repository
git clone https://github.com/khantseithu/http-server-in-c.git
cd http-server-in-c

# Install OpenSSL (macOS/Homebrew)
brew install openssl

# Build (adjust OpenSSL paths in Makefile if needed)
make build
```

---

## 🔧 Configuration

### HTTPS Certificates

1. **Development** (self-signed):

   ```bash
   mkdir -p certs
   openssl req -x509 -newkey rsa:4096 -keyout certs/key.pem -out certs/cert.pem -days 365 -nodes
   ```

2. **Production** (Let’s Encrypt):
   ```bash
   certbot certonly --standalone -d yourdomain.com
   # Symlink to project certs/
   ln -s /etc/letsencrypt/live/yourdomain.com/ certs/production
   ```

### Environment Variables

| Variable     | Default          | Description      |
| ------------ | ---------------- | ---------------- |
| `PORT`       | `8080`           | HTTP port        |
| `HTTPS_PORT` | `8443`           | HTTPS port       |
| `SSL_CERT`   | `certs/cert.pem` | Certificate path |
| `SSL_KEY`    | `certs/key.pem`  | Private key path |

---

## 🚀 Usage

### Start Server

```bash
# HTTP only
./server

# HTTPS (with certs)
./server --https
```

### Example Requests

```bash
# Fetch HTML page
curl -k https://localhost:8443/

# Test security headers
curl -I https://localhost:8443/
```

---

## 🛡️ Security Best Practices

- 🔒 **Never commit certificate files** (add to `.gitignore`)
- 🔄 Automate Let’s Encrypt renewal:
  ```bash
  certbot renew --quiet --post-hook "killall server && make run"
  ```
- 🚫 Use firewall rules to restrict port access
- 📈 Monitor with tools like `fail2ban` or `nginx` reverse proxy

---

## 🧑💻 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feat/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feat/amazing-feature`)
5. Open a Pull Request

---

## 📄 License

Distributed under the MIT License. See `LICENSE` for details.

---

## 🙏 Acknowledgments

- OpenSSL for TLS/SSL implementation
- Inspired by [tinyhttp](https://github.com/tinyhttp/tinyhttp) and [libhv](https://github.com/ithewei/libhv)
