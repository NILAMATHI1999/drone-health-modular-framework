from setuptools import setup

package_name = 'drone_health_dashboard'

setup(
    name=package_name,
    version='0.1.0',
    packages=[],
    py_modules=[],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/web', [
            'web/index.html',
            'web/styles.css',
            'web/app.js',
         ]),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Nila',
    maintainer_email='nila@example.com',
    description='Web dashboard and ROS bridge for drone health monitoring.',
    license='MIT',
    scripts=[
        'scripts/dashboard_bridge.py',
    ],
)

