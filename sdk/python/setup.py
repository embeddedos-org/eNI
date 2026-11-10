from setuptools import setup, find_packages

setup(
    name="eni",
    version="0.2.0",
    description="eNI Python SDK — Neural Interface bindings",
    long_description=open("../../README.md").read() if __import__("os").path.exists("../../README.md") else "",
    long_description_content_type="text/markdown",
    author="EoS Project",
    license="MIT",
    packages=find_packages(),
    python_requires=">=3.8",
    # eni/core.py imports numpy at module level, so "import eni" needs it.
    install_requires=["numpy>=1.20"],
    extras_require={
        "numpy": ["numpy>=1.20"],
        "viz": ["matplotlib>=3.5", "numpy>=1.20"],
        "full": ["numpy>=1.20", "matplotlib>=3.5", "plotly>=5.0", "pandas>=1.4"],
    },
    classifiers=[
        "Development Status :: 3 - Alpha",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Topic :: Scientific/Engineering :: Human Machine Interfaces",
    ],
)
