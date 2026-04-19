# Encore Karaoke Assets

This directory contains all media assets for the Encore Karaoke application.

## Directory Structure

### 📺 **emojis/**
- **Purpose**: Emoji graphics displayed on the lyric screen
- **Usage**: When customers send emojis to singers during performances
- **Format**: PNG/SVG emoji images
- **Display**: Shown on public lyric display screen

### 🔤 **fonts/**
- **Purpose**: Custom fonts for the application UI and lyric display
- **Usage**: Typography for titles, lyrics, menus, and branding
- **Format**: TTF/OTF font files
- **Categories**: UI fonts, lyric fonts, decorative fonts

### 👤 **icon/** 
- **Purpose**: Avatar icons for singer stage names
- **Usage**: Displayed in the queue to represent each singer
- **Format**: PNG/JPG avatar images
- **Display**: Singer queue, now playing, history screens

### 🖼️ **images/**
- **Purpose**: General application images and graphics
- **Usage**: Backgrounds, logos, UI elements, promotional graphics
- **Format**: PNG/JPG/SVG images
- **Categories**: Backgrounds, logos, buttons, decorative elements

### 🎵 **music/**
- **Purpose**: Background music tracks
- **Usage**: Played between singer performances to fill silence
- **Format**: MP3/WAV/FLAC audio files
- **Categories**: Ambient tracks, intermission music, venue atmosphere

### 🔊 **sound-icons/**
- **Purpose**: Visual icons for soundboard buttons
- **Usage**: Button graphics for the DJ soundboard interface
- **Format**: PNG/SVG icon images
- **Categories**: Sound effect icons, audio control icons

### 🎶 **sounds/**
- **Purpose**: Audio files for the soundboard
- **Usage**: Sound effects triggered by DJ during performances
- **Format**: MP3/WAV audio files
- **Categories**: Applause, air horn, drums, comedy effects, crowd sounds

## Asset Guidelines

- **Naming**: Use descriptive, lowercase names with dashes (e.g., `crowd-cheer.mp3`)
- **Quality**: High-resolution for images, high-quality audio for sounds
- **Licensing**: Ensure all assets are properly licensed for commercial use
- **Organization**: Group related assets in subfolders when needed
- **Size**: Optimize file sizes for performance while maintaining quality

## Integration

Assets are loaded by the application at runtime and can be referenced in code using relative paths from the assets directory. The build system automatically includes these assets in the final application package.