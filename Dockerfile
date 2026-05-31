# ═══════════════════════════════════════════════════════════
# Multi-stage Dockerfile — Python service
# ═══════════════════════════════════════════════════════════
FROM python:3.12-slim AS base
WORKDIR /app
COPY requirements*.txt ./
RUN pip install --no-cache-dir -r requirements.txt 2>/dev/null || true
COPY . .
RUN pip install --no-cache-dir -e . 2>/dev/null || true

FROM base AS test-runner
RUN pip install --no-cache-dir pytest pytest-cov
CMD ["python", "-m", "pytest", "tests/", "-v", "--tb=short"]

FROM base AS production
EXPOSE 8080
CMD ["python", "-m", "uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8080"]
