# Prompt: tady jsou materiály k projektu z IFJ. chci aby jsi použil šablonu a upravil ji tak, aby byla ready na tento projekt. nechci v ní nic zbytečně navíc
FROM gcc:16-trixie
RUN apt-get update && apt-get install -y --no-install-recommends \
    make python3 clang-format cppcheck valgrind git ca-certificates doxygen graphviz \
    && rm -rf /var/lib/apt/lists/*
ENV HOME=/tmp
WORKDIR /workspace
CMD ["bash"]
