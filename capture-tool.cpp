#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QDateTime>
#include <QCoreApplication>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <chrono>
#include <QComboBox>
#include <QSize>

using namespace std;
using namespace cv;

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle("Capture Tool");
    window.resize(800, 600);

    QLabel *feedLabel = new QLabel(&window);
    feedLabel->setMinimumSize(640, 480);
    feedLabel->setAlignment(Qt::AlignCenter);
    feedLabel->setStyleSheet("background-color: black; color: white;");
    feedLabel->setText("Starting camera initialization...");

    QLabel *statusLabel = new QLabel("Ready", &window);

    QSpinBox *countdownBox = new QSpinBox(&window);
    countdownBox->setRange(0, 15);
    countdownBox->setPrefix("Countdown: ");
    countdownBox->setSuffix(" seconds");
    countdownBox->setValue(3);
    countdownBox->setToolTip("Delay in seconds before the video recording actually starts.");

    QSpinBox *durationBox = new QSpinBox(&window);
    durationBox->setRange(1, 120);
    durationBox->setPrefix("Duration: ");
    durationBox->setSuffix(" seconds");
    durationBox->setValue(5);
    durationBox->setToolTip("Length of the video to record in seconds.");

    QPushButton *captureBtn = new QPushButton("Capture Single Frame", &window);
    captureBtn->setToolTip("Takes a single snapshot from the camera and saves it as a JPG image.");
    
    QPushButton *recordBtn = new QPushButton("Record Video", &window);
    recordBtn->setToolTip("Starts recording an AVI video after the countdown finishes.");

    QComboBox *resolutionBox = new QComboBox(&window);
    resolutionBox->addItem("1080p (1920 x 1080)", QSize(1920, 1080));
    resolutionBox->addItem("720p (1280 x 720)", QSize(1280, 720));
    resolutionBox->addItem("480p (640 x 480)", QSize(640, 480));
    // Set default to 720p
    resolutionBox->setCurrentIndex(1);
    resolutionBox->setToolTip("Select the camera's capture resolution.");

    QComboBox *formatBox = new QComboBox(&window);
    formatBox->addItem("JPG");
    formatBox->addItem("PNG");
    formatBox->addItem("BMP");
    formatBox->addItem("TIFF");
    formatBox->setToolTip("Select the file format for captured frames.");

    QHBoxLayout *controlsLayout = new QHBoxLayout;
    controlsLayout->addWidget(resolutionBox);
    controlsLayout->addWidget(formatBox);
    controlsLayout->addWidget(countdownBox);
    controlsLayout->addWidget(durationBox);
    controlsLayout->addWidget(captureBtn);
    controlsLayout->addWidget(recordBtn);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(feedLabel);
    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(statusLabel);
    window.setLayout(mainLayout);

    VideoCapture cap;
    int cameraFps = 30;
    int cameraWidth = 1280;
    int cameraHeight = 720;

    auto openCamera = [&](int width, int height) -> bool {
        if (cap.isOpened()) {
            cap.release();
        }

        string pipeline = "libcamerasrc af-mode=continuous ! video/x-raw, width=" + to_string(width) + 
                          ", height=" + to_string(height) + ", framerate=" + to_string(cameraFps) + 
                          "/1, format=RGBx ! videoconvert ! video/x-raw, format=BGR ! appsink drop=true sync=false";
        
        cap.open(pipeline, CAP_GSTREAMER);
        
        if (!cap.isOpened()) {
            cap.open(0, CAP_V4L2);
            if (cap.isOpened()) {
                cap.set(CAP_PROP_FRAME_WIDTH, width);
                cap.set(CAP_PROP_FRAME_HEIGHT, height);
                cap.set(CAP_PROP_AUTOFOCUS, 1);
            }
        }

        if (cap.isOpened()) {
            cameraWidth = width;
            cameraHeight = height;
            statusLabel->setText(QString("Camera at %1 x %2").arg(cameraWidth).arg(cameraHeight));
            return true;
        }

        return false;
    };

    if (!openCamera(cameraWidth, cameraHeight)) {
        feedLabel->setText("Fatal Error: Camera module not accessible.");
        return -1;
    }

    QTimer *timer = new QTimer(&window);
    VideoWriter writer;
    bool isRecording = false;
    int targetFrames = 0;
    int framesRecorded = 0;

    QObject::connect(resolutionBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int index) {
        if (isRecording) {
            statusLabel->setText("Cannot change resolution while recording.");
            return;
        }
        QSize selectedSize = resolutionBox->itemData(index).toSize();
        timer->stop();
        feedLabel->setText("Switching camera resolution...");
        QCoreApplication::processEvents();

        if (!openCamera(selectedSize.width(), selectedSize.height())) {
            feedLabel->setText("Failed to switch camera resolution.");
            statusLabel->setText("Camera reopen failed.");
        }
        timer->start(1000 / cameraFps);
    });

    QObject::connect(timer, &QTimer::timeout, [&]() {
        Mat frame;
        cap.read(frame);
        if (frame.empty()) return;

        if (isRecording) {
            writer.write(frame);
            framesRecorded++;
            statusLabel->setText(QString("Recording active: %1 / %2 frames").arg(framesRecorded).arg(targetFrames));
            
            if (framesRecorded >= targetFrames) {
                isRecording = false;
                writer.release();
                recordBtn->setEnabled(true);
                captureBtn->setEnabled(true);
                resolutionBox->setEnabled(true);
                statusLabel->setText("Recorded Video successfully saved to this folder.");
            }
        }

        Mat rgbFrame;
        cvtColor(frame, rgbFrame, COLOR_BGR2RGB);
        QImage img(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step, QImage::Format_RGB888);
        feedLabel->setPixmap(QPixmap::fromImage(img).scaled(feedLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
    
    timer->start(1000 / cameraFps); 

    QObject::connect(captureBtn, &QPushButton::clicked, [&]() {
        Mat frame;
        cap.read(frame);
        if (!frame.empty()) {
            QString ext = formatBox->currentText().toLower();
            QString filename = "frame_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "." + ext;
            imwrite(filename.toStdString(), frame);
            statusLabel->setText("Captured and saved " + filename + " to this folder.");
        }
    });

    QObject::connect(recordBtn, &QPushButton::clicked, [&]() {
        recordBtn->setEnabled(false);
        captureBtn->setEnabled(false);
        resolutionBox->setEnabled(false);
        int countdown = countdownBox->value();
        int duration = durationBox->value();

        for (int i = countdown; i > 0; i--) {
            statusLabel->setText(QString("Recording begins in %1 seconds...").arg(i));
            QCoreApplication::processEvents();
            this_thread::sleep_for(chrono::seconds(1));
        }

        targetFrames = duration * 30; 
        framesRecorded = 0;
        QString filename = "video_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".avi";
        
        writer.open(filename.toStdString(), VideoWriter::fourcc('M','J','P','G'), cameraFps, Size(cameraWidth, cameraHeight));

        if (writer.isOpened()) {
            isRecording = true;
            statusLabel->setText("Recording initiated.");
        } else {
            statusLabel->setText("Error creating the video file payload.");
            recordBtn->setEnabled(true);
            captureBtn->setEnabled(true);
            resolutionBox->setEnabled(true);
        }
    });

    window.show();
    return app.exec();
}
