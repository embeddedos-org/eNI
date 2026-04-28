"""Generate narration audio using Google Text-to-Speech."""
from gtts import gTTS

NARRATION = (
    "Introducing eNI. The embedded neural interface framework. Feature one: Cross-platform neural I/O connects to any brain-computer interface hardware. Feature two: Real-time signal processing handles neural data at millisecond precision. Feature three: Hardware abstraction layer supports multiple sensor protocols. eNI. Open source and cutting edge. Visit github dot com slash embeddedos-org slash eNI."
)

tts = gTTS(text=NARRATION, lang="en", slow=False)
tts.save("narration.mp3")
print(f"Generated narration.mp3 ({len(NARRATION)} chars)")
